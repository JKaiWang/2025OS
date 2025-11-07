# HW1 – Inter-Process Communication Using Message Passing and Shared Memory

## Introduction
This assignment focuses on understanding **Inter-Process Communication (IPC)** — the mechanisms that allow multiple processes to exchange data and synchronize their execution. Since processes in modern operating systems run in **separate memory spaces**, they cannot directly access each other's data. Therefore, the operating system provides IPC mechanisms to enable controlled and organized communication.

To explore these concepts, this project implements two processes:
- **Sender** – responsible for producing and sending messages.
- **Receiver** – responsible for receiving and printing messages.

The system supports two different IPC approaches:
1. **Message Passing** using System V Message Queues
2. **Shared Memory** using an allocated memory segment accessible by both processes

Additionally, **POSIX semaphores** are used to synchronize communication and avoid race conditions.

---

## Background Knowledge

### 1. Inter-Process Communication (IPC)
IPC refers to a set of OS-supported techniques that allow processes to exchange data. Without IPC, multiple processes cannot collaborate, coordinate work, or share results. Common IPC mechanisms include:
- Pipes
- Message Queues
- Shared Memory
- Signals
- Sockets

This assignment focuses on **Message Queues** and **Shared Memory**, representing two different communication philosophies.

---

### 2. Message Passing
Message passing treats communication as sending complete messages through a **kernel-managed queue**.  
The sender places a message into the queue, and the receiver retrieves it when ready.  
Key properties:
- No memory is shared.
- The operating system manages storage and synchronization.
- Safe and simple, but performance may be slower due to system calls and data copying.

This model resembles a **postal mailbox system**.

---

### 3. Shared Memory
Shared memory allows two processes to directly access the **same memory region**.  
This approach avoids copying data and is generally faster, but it requires **explicit synchronization** to avoid concurrent access problems.

This model resembles **two people writing on the same whiteboard** — very efficient, but only if they take turns properly.

---

### 4. Semaphores for Synchronization
Because shared memory does not enforce any access rules, **semaphores** are used to coordinate the timing of send/receive actions.  
Semaphores are counters managed by the OS that support:
- **wait** (block until permitted)
- **signal** (grant permission to continue)

In this assignment:
- One semaphore ensures the **sender** waits until the receiver has processed the previous message.
- Another semaphore ensures the **receiver** waits until the sender has produced new data.

This prevents:
- Reading uninitialized data
- Overwriting unread data
- Busy waiting

---

## How the System Works

### Communication Flow Overview
Both sender and receiver start by establishing:
- A **communication channel** (either a message queue or shared memory block)
- **Shared semaphores** for synchronization

Then the data exchange proceeds as a repeating cycle:

