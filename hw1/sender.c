#define _POSIX_C_SOURCE 199309L
#include "sender.h"
#include <unistd.h>
#include <semaphore.h>
#include <errno.h>
#include <string.h>

#define BLUE  "\033[1;34m"
#define YELLOW "\033[1;33m"
#define RED "\033[1;31m"
#define GREEN "\033[1;32m"
#define RESET "\033[0m"

struct timespec start, end;
double total_time = 0.0;

sem_t *sender_sem = NULL;
sem_t *receiver_sem = NULL;
//
void send(message_t message, mailbox_t *mailbox_ptr) // message: message 結構（傳值）傳送要送出的內容 mailbox_ptr: 指向要儲存IPC的設定（queueID or shared memory address）

{
    // 等待 receiver 訊號（不計時）
    //sem_wair()會讓sender 在semaphore 值為0 導致stuck
    //一開始sender initial value 為1 所以第一次會直接通過
    //接下來要等receiver 收完一封信，sem_post(sender_sem)通之後才會放行
    sem_wait(sender_sem);

    if (mailbox_ptr->flag == MSG_PASSING)
    {
        clock_gettime(CLOCK_MONOTONIC, &start);
        //msgsnd()參數
        // mailbox_ptr->storage.msqid : QueueID （由msgget()建立）
        // &message -> 要傳送的訊息
        //sizeof(message.msgtText) ->資料的大小
        // 0 ->預設阻塞模式(會等queue 可用)
        if (msgsnd(mailbox_ptr->storage.msqid, &message, sizeof(message.msgText), 0) == -1) 
        {
            perror("msgsnd failed");
            exit(1);
        }
        clock_gettime(CLOCK_MONOTONIC, &end);
    }
    else if (mailbox_ptr->flag == SHARED_MEM)
    {
        clock_gettime(CLOCK_MONOTONIC, &start);
        //取出共享記憶體的起始位置
        //shm[0] =1 有新資料
        
        char *shm = mailbox_ptr->storage.shm_addr;
        shm[0] = 1;
        //把文字訊息複製進去共享記憶體中（從地2byte開始）
        strcpy(shm + 1, message.msgText);
        clock_gettime(CLOCK_MONOTONIC, &end);
    }
    else
    {
        fprintf(stderr, "Invalid communication flag.\n");
        exit(1);
    }

    sem_post(receiver_sem); // 通知 receiver 可以收
    total_time += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
}

int main(int argc, char *argv[]){
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <mode> <input.txt>\n", argv[0]);
        exit(1);
    }

    int mode = atoi(argv[1]);
    char *filename = argv[2];
    mailbox_t mailbox;
    mailbox.flag = mode;

    // 建立IPC 共用Key 讓 receiver and sencer 都可以找到同一個message queue 或shared memory 
    // ftok: File to Key ：他會根據「檔案路徑+ 一個編號」產生一個獨一無二的key
    key_t key = ftok(".", 65);
    if (key == -1)
    {
        perror("ftok failed");
        exit(1);
    }

    // 建立 IPC
    if (mode == MSG_PASSING) //using System V message queue to send the message
    {
        printf(BLUE"Message Passing\n"RESET);
        //建立一個System V message queue 
        //key: 由ftok()產生唯一識別碼
        //IPC_CREAT: 若queue 不存在就建立一個
        //0666: 權限設定(read and write for everyone)
        //這樣一來sender and receiver 就可以用相同的key 連線到同一個queue
        int msqid = msgget(key, IPC_CREAT | 0666);
        if (msqid == -1)
        {
            perror("msgget failed");
            exit(1);
        }
        //把queue ID 存到mailbox結構裡 之後 msgsnd() 或是 msgrcv() 都會用到它
        mailbox.storage.msqid = msqid;
    }
    else if (mode == SHARED_MEM)
    {
        printf(BLUE"Shared Memory\n"RESET);
        // 建立一個共同記憶體區段(Shared Memory Segment)
        //key: generate by ftok() , 確保sender / receiver 共用同一塊記憶體
        //1025: 區段大小(單位是byte)這裡設1025 剛好可以放一個旗標加上一個1024的訊息

        int shmid = shmget(key, 1025, IPC_CREAT | 0666);
        if (shmid == -1)
        {
            perror("shmget failed");
            exit(1);
        }
        //shmat(): attach shared memory 
        //這會將剛剛建立的shared memory 區段「掛載」到這個process 的記憶體空間
        //回傳一個指標(char*)讓程式能直接存取這塊記憶體
        //shmid: share memory ID
        //NULL : 讓OS 自動決定要掛在哪
        //0: 預設模式（read and write）
        char *shm = (char *)shmat(shmid, NULL, 0);
        if (shm == (char *)-1)
        {
            perror("shmat failed");
            exit(1);
        }
        //將指標存取進去mailbox 中
        mailbox.storage.shm_addr = shm;
        //init shared memory 的狀態旗標，第一個byte 作為同步旗標
        // 0 = emty (receiver can wait for new message) 1= there is new message(sender written)
        shm[0] = 0;
    }
    else
    {
        fprintf(stderr, "Invalid mode. Use 1 for Message Passing, 2 for Shared Memory.\n");
        exit(1);
    }



    // 建立 semaphore（都初始化為 0）
    //sem_open() 它是POSIX semaphore(命名訊號量)的函式，用來讓兩個不同的process 共享同步機制
    //語法： sem_t* sem_open(const char name , int oflag , mode_t mode , unsigned int value)
    //name: semaphore 的名稱(跨程式共用)
    // O_CREAT 若semaphore 不存在就建立新的 
    // 0644 : 權限 (user 可讀可寫 ，其他人可讀)
    // value 初始值（表示目前有多少資源可以用）
    //在這個程式底下 sender_sem = 1 receiver_sem = 0 代表sender 一開始可以接受訊息因為沒有東西要等，而receiver 要等待sender
    
    /*
        sender 開始 actions: sem_wait(sender_sem) 通過（1->0）
        sender 傳完訊息 actions : sem_post(receiver_sem) ->receiver_sem = 1 (notify receiver)
        receiver接收完訊息： sem_post(sender_sem) -> sender_sem =1 （通知sender可以再送)
        雙方會輪流執行
    */ 
    sender_sem = sem_open("/sender_sem", O_CREAT, 0644, 1);
    receiver_sem = sem_open("/receiver_sem", O_CREAT, 0644, 0);
    if (sender_sem == SEM_FAILED || receiver_sem == SEM_FAILED)
    {
        perror("sem_open failed");
        exit(1);
    }

    FILE *fp = fopen(filename, "r");
    if (!fp)
    {
        perror("Cannot open input file.");
        exit(1);
    }

    char buffer[1024];
    message_t msg;
    msg.mType = 1;

    while (fgets(buffer, sizeof(buffer), fp))
    {
        buffer[strcspn(buffer, "\n")] = '\0';
        strcpy(msg.msgText, buffer);
        send(msg, &mailbox);
        printf(BLUE"Sending message: "RESET"%s\n", msg.msgText);
    }

    strcpy(msg.msgText, "exit");
    send(msg, &mailbox);
    printf(RED "End of input file! exit!\n" RESET);

    printf("Total time taken in sending msg: %.9f s\n", total_time);

    fclose(fp);
    if (mode == SHARED_MEM)
        shmdt(mailbox.storage.shm_addr);

    sem_close(sender_sem);
    sem_close(receiver_sem);
    return 0;
}



