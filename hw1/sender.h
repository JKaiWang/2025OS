#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <time.h>

//定義兩種通訊模式 (message passing and shared memory mode)
#define MSG_PASSING 1
#define SHARED_MEM 2



typedef struct {
    int flag;      // 1 for message passing, 2 for shared memory
    union{
        int msqid; //for system V api. You can replace it with structure for POSIX api
        char* shm_addr; // shared memory 的address pointer 
    }storage;
} mailbox_t;


typedef struct {
    /*  TODO: 
        Message structure for wrapper
    */
    long mType; // 訊息類別：有兩種 1. msgsnd() 和 2. msgrcv()
    char msgText[1024]; // 實際文字內容
} message_t;

void send(message_t message, mailbox_t* mailbox_ptr);


/*
    1. msgsnd() 把一個訊息放入在message queue 中，讓其他程式可以之取出
        function 的原型 int msgsnd(int msqid , const void* msgp , size_t msgsz , int msgflg)
        * msqid: queue ID 由 msgget() 建立或取得
        * msgp: 指向訊息結構的指標(結構低一蘭必須是long 型別的mtype)
        * msgsz: 訊息正文(不含mtpye 大小)
        * msgflg: 控制其標(例如：IPC_NOWAIT 代表非阻塞模式) 
    2. msgrcv() 從message queue 中取出一個訊息
        function 原型： ssize_t msgrcv(int msqid , void *msgp , size_t msgsz , long msgtype , int msgflg)
        * msqid: queue ID 
        * msgp: 指向用來存放接收資料的結構體
        * msgsz: 要讀取的訊息正文大小(不含mtype)
        * msgtyp: 指定要接受哪一種類型的訊息
        * msgflg: 控制其標
*/