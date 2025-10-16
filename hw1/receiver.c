#define _POSIX_C_SOURCE 199309L
#include "receiver.h"
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <semaphore.h>

struct timespec start, end;
double total_time = 0.0;

sem_t *sender_sem = NULL;
sem_t *receiver_sem = NULL;

void receive_message(message_t *message_ptr, mailbox_t *mailbox_ptr)
{
    sem_wait(receiver_sem); // 等待 sender 通知（不計時）

    if (mailbox_ptr->flag == MSG_PASSING)
    {
        clock_gettime(CLOCK_MONOTONIC, &start);
        if (msgrcv(mailbox_ptr->storage.msqid, message_ptr, sizeof(message_ptr->msgText), 1, 0) == -1)
        {
            perror("msgrcv failed");
            exit(1);
        }
        clock_gettime(CLOCK_MONOTONIC, &end);
    }
    else if (mailbox_ptr->flag == SHARED_MEM)
    {
        char *shm = mailbox_ptr->storage.shm_addr;
        clock_gettime(CLOCK_MONOTONIC, &start);
        strcpy(message_ptr->msgText, shm + 1);
        clock_gettime(CLOCK_MONOTONIC, &end);
        shm[0] = 0;
    }

    total_time += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    sem_post(sender_sem); // 通知 sender 可以送下一封
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Usage: ./receiver <mode>\n");
        return 1;
    }

    int mode = atoi(argv[1]);
    mailbox_t mailbox;
    message_t message;
    mailbox.flag = mode;

    key_t key = ftok(".", 65);
    if (key == -1)
    {
        perror("ftok failed");
        exit(1);
    }

    if (mode == MSG_PASSING)
    {
        mailbox.storage.msqid = msgget(key, 0666 | IPC_CREAT);
        if (mailbox.storage.msqid == -1)
        {
            perror("msgget failed");
            exit(1);
        }
    }
    else if (mode == SHARED_MEM)
    {
        int shmid = shmget(key, 1025, 0666 | IPC_CREAT);
        mailbox.storage.shm_addr = (char *)shmat(shmid, NULL, 0);
        if (mailbox.storage.shm_addr == (char *)-1)
        {
            perror("shmat failed");
            exit(1);
        }
    }
    else
    {
        fprintf(stderr, "Invalid mode. Use 1 for Message Passing, 2 for Shared Memory.\n");
        exit(1);
    }

    sender_sem = sem_open("/sender_sem", 0);
    receiver_sem = sem_open("/receiver_sem", 0);
    if (sender_sem == SEM_FAILED || receiver_sem == SEM_FAILED)
    {
        perror("sem_open failed");
        exit(1);
    }

    // receiver 啟動後立即通知 sender 可以送第一封
    sem_post(sender_sem);

    while (1)
    {
        receive_message(&message, &mailbox);
        printf("Receiver: received message \"%s\"\n", message.msgText);

        if (strcmp(message.msgText, "exit") == 0)
        {
            sem_post(sender_sem); // prevent sender stuck
            break;
        }
    }

    printf("Receiver: total receiving time = %.9f seconds\n", total_time);

    if (mode == SHARED_MEM)
    {
        shmdt(mailbox.storage.shm_addr);
        int shmid = shmget(key, 1025, 0666);
        shmctl(shmid, IPC_RMID, NULL);
    }
    else
    {
        msgctl(mailbox.storage.msqid, IPC_RMID, NULL);
    }

    sem_close(sender_sem);
    sem_close(receiver_sem);
    sem_unlink("/sender_sem");
    sem_unlink("/receiver_sem");

    return 0;
}