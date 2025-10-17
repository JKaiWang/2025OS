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
void send_message(message_t message, mailbox_t *mailbox_ptr)
{
    // 等待 receiver 訊號（不計時）
    sem_wait(sender_sem);

    if (mailbox_ptr->flag == MSG_PASSING)
    {
        clock_gettime(CLOCK_MONOTONIC, &start);
        if (msgsnd(mailbox_ptr->storage.msqid, &message, sizeof(message.msgText), 0) == -1)
        {
            perror("msgsnd failed");
            exit(1);
        }
        clock_gettime(CLOCK_MONOTONIC, &end);
    }
    else if (mailbox_ptr->flag == SHARED_MEM)
    {
        char *shm = mailbox_ptr->storage.shm_addr;
        clock_gettime(CLOCK_MONOTONIC, &start);
        shm[0] = 1;
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

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <mode> <input.txt>\n", argv[0]);
        exit(1);
    }

    int mode = atoi(argv[1]);
    char *filename = argv[2];
    mailbox_t mailbox;
    mailbox.flag = mode;

    key_t key = ftok(".", 65);
    if (key == -1)
    {
        perror("ftok failed");
        exit(1);
    }

    // 建立 IPC
    if (mode == MSG_PASSING)
    {
        printf(BLUE"Message Passing\n"RESET);
        int msqid = msgget(key, IPC_CREAT | 0666);
        if (msqid == -1)
        {
            perror("msgget failed");
            exit(1);
        }
        mailbox.storage.msqid = msqid;
    }
    else if (mode == SHARED_MEM)
    {
        printf(BLUE"Shared Memory\n"RESET);
        int shmid = shmget(key, 1025, IPC_CREAT | 0666);
        if (shmid == -1)
        {
            perror("shmget failed");
            exit(1);
        }
        char *shm = (char *)shmat(shmid, NULL, 0);
        if (shm == (char *)-1)
        {
            perror("shmat failed");
            exit(1);
        }
        mailbox.storage.shm_addr = shm;
        shm[0] = 0;
    }
    else
    {
        fprintf(stderr, "Invalid mode. Use 1 for Message Passing, 2 for Shared Memory.\n");
        exit(1);
    }

    // 建立 semaphore（都初始化為 0）
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
        send_message(msg, &mailbox);
        printf(BLUE"Sending message: "RESET"%s\n", msg.msgText);
    }

    strcpy(msg.msgText, "exit");
    send_message(msg, &mailbox);
    printf(RED "End of input file! exit!\n" RESET);

    printf("Total time taken in sending msg: %.9f s\n", total_time);

    fclose(fp);
    if (mode == SHARED_MEM)
        shmdt(mailbox.storage.shm_addr);

    sem_close(sender_sem);
    sem_close(receiver_sem);
    return 0;
}