#define _POSIX_C_SOURCE 199309L
#include "receiver.h"
#include <time.h>
#include <unistd.h>
#include <string.h>

void receive(message_t* message_ptr, mailbox_t* mailbox_ptr) {
    if (mailbox_ptr->flag == MSG_PASSING) {
        // 📨 Message Queue 模式
        if (msgrcv(mailbox_ptr->storage.msqid, message_ptr, sizeof(message_ptr->msgText), 1, 0) == -1) {
            perror("msgrcv failed");
            exit(1);
        }

    } else if (mailbox_ptr->flag == SHARED_MEM) {
        // 🧠 Shared Memory 模式
        char* shm = mailbox_ptr->storage.shm_addr;

        // 等待 sender 寫入 (flag == 1)
        while (shm[0] == 0) {
            sleep(1); // 1ms delay 避免 busy loop
        }

        // 複製內容
        strcpy(message_ptr->msgText, shm + 1);

        // 清空 flag（通知 sender 可以寫下一筆）
        shm[0] = 0;
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: ./receiver <mode>\n");
        return 1;
    }

    int mode = atoi(argv[1]);
    mailbox_t mailbox;
    message_t message;
    struct timespec start, end;
    double total_time = 0.0;
    int first_message = 1;  

    mailbox.flag = mode;

    key_t key = ftok(".", 65); //與 sender 一致

    // 初始化 IPC
    if (mode == MSG_PASSING) {
        mailbox.storage.msqid = msgget(key, 0666 | IPC_CREAT);
        if (mailbox.storage.msqid == -1) {
            perror("msgget failed");
            exit(1);
        }
    } else if (mode == SHARED_MEM) {
        int shmid = shmget(key, 1024 + 1, 0666 | IPC_CREAT);
        mailbox.storage.shm_addr = (char*)shmat(shmid, NULL, 0);
        if (mailbox.storage.shm_addr == (char*)-1) {
            perror("shmat failed");
            exit(1);
        }
    } else {
        fprintf(stderr, "Invalid mode. Use 1 for Message Passing, 2 for Shared Memory.\n");
        exit(1);
    }

    //  主迴圈
    while (1) {
        // ⏱ 計時 receive 階段
        clock_gettime(CLOCK_MONOTONIC, &start);
        receive(&message, &mailbox);
        clock_gettime(CLOCK_MONOTONIC, &end);

        // 第一次收到才開始累計時間（略過待機時間）
        if (first_message) {
            total_time = 0.0;
            first_message = 0;
        }

        double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;
        total_time += elapsed;

        printf("Receiver: received message \"%s\"\n", message.msgText);

        // 收到 exit → 結束
        if (strcmp(message.msgText, "exit") == 0) {
            break;
        }
    }

    printf("Receiver: total receiving time = %.9f seconds\n", total_time);

    //  清理資源
    if (mode == SHARED_MEM) {
        shmdt(mailbox.storage.shm_addr);
        shmctl(shmget(key, 0, 0), IPC_RMID, NULL);
    } else {
        msgctl(mailbox.storage.msqid, IPC_RMID, NULL);
    }

    return 0;
}
