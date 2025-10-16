#define _POSIX_C_SOURCE 199309L
#include "sender.h"
#include<unistd.h>

void send(message_t message, mailbox_t* mailbox_ptr) {
    if (mailbox_ptr->flag == MSG_PASSING) {
        // 📨 Message Passing
        if (msgsnd(mailbox_ptr->storage.msqid, &message, sizeof(message.msgText), 0) == -1) {
            perror("msgsnd failed");
            exit(1);
        }

    } else if (mailbox_ptr->flag == SHARED_MEM) {
        //  Shared Memory
        char* shm = mailbox_ptr->storage.shm_addr;
        // 等待上一筆資料被 receiver 清空
        while (shm[0] == 1) {
            sleep(1); // 1ms delay to avoid busy waiting
        }

        shm[0] = 1; // flag = filled
        strcpy(shm + 1, message.msgText);

    } else {
        fprintf(stderr, "Invalid communication flag.\n");
        exit(1);
    }
}

int main(int argc , char* argv[]){
    if(argc !=3){
        fprintf(stderr,"Usage: %s <mode> <inpiut.txt>\n" , argv[0]);
        exit(1);
    }
    int mode =atoi(argv[1]);
    char* filename = argv[2];

    mailbox_t mailbox;
    mailbox.flag = mode;

    key_t key =ftok("." , 65);

    if(mode == MSG_PASSING){
        int msqid = msgget(key , IPC_CREAT | 0666);
        if(msqid == -1){
            perror("msgget failed");
            exit(1);
        }
        mailbox.storage.msqid = msqid;
    }
    else if(mode == SHARED_MEM){
        int shmid = shmget(key , 1024+1 , IPC_CREAT|0666);
        if(shmid == -1){
            perror("shmget failed");
            exit(1);
        }
        char* shm = (char*)shmat(shmid , NULL , 0);
        if(shm == (char*)-1){
            perror("shmat failed");
            exit(1);
        }
        mailbox.storage.shm_addr = shm;
        shm[0] = 0;
    }
    else{
        fprintf(stderr , "Invalid mode. Use 1 for Message passage, 2 for Shared Memory.\n");
        exit(1);
    }
    FILE* fp = fopen(filename , "r");
    if(!fp){
        perror("Cannot open input file.");
        exit(1);
    }

    

    char buffer[1024];
    message_t msg;
    msg.mType =1;

    struct timespec start , end;
    double total_time = 0.0;

    while(fgets(buffer , sizeof(buffer) , fp)){
        buffer[strcspn(buffer , "\n")] = '\0';
        strcpy(msg.msgText , buffer);
        clock_gettime(CLOCK_MONOTONIC , &start);
        send(msg , &mailbox);
        clock_gettime(CLOCK_MONOTONIC , &end);

        double elapsed = ((end.tv_sec - start.tv_sec)+(end.tv_nsec - start.tv_nsec)*1e-9);
        total_time += elapsed;
        printf("[Sender] Sent message: %s\n" , msg.msgText);
    }

    strcpy(msg.msgText,"exit");
    clock_gettime(CLOCK_MONOTONIC, &start);
    send(msg , &mailbox);
    clock_gettime(CLOCK_MONOTONIC , &end);
    total_time+= (end.tv_sec - start.tv_sec)+(end.tv_nsec -start.tv_nsec)*1e-9;

    printf("[Sender] Sent exit message.\n");

    printf("[Sender] Total sending time: %.9f seconds\n" , total_time);

    fclose(fp);
    if(mode == SHARED_MEM){
        shmdt(mailbox.storage.shm_addr);
    }
    return 0;

    
}
