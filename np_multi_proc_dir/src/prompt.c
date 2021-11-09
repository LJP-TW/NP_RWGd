#include <unistd.h>
#include <string.h>
#include <sys/sem.h>
#include <sys/shm.h>

#include "prompt.h"
#include "netio.h"

message_buf global_msg;

void global_msg_init(void)
{
    global_msg.shmid = shmget(IPC_PRIVATE, sizeof(char) * MAX_MSG_LEN, \
                              IPC_CREAT | IPC_EXCL | 0600);
    
    global_msg.msg = shmat(global_msg.shmid, NULL, 0);

    global_msg.msg[0] = 0;

    // Semaphore of user_manager
    global_msg.semid = semget(IPC_PRIVATE, 2, IPC_CREAT | IPC_EXCL | 0600);
}

void global_msg_release(void)
{
    semctl(global_msg.semid, 0, IPC_RMID);
    shmctl(global_msg.shmid, IPC_RMID, NULL);
}

void msg_write_wait(void)
{
    struct sembuf ops[] = {
        {0, 0, 0},        // wait until write_sem == 0
        {0, 1, SEM_UNDO}, // write_sem += 1
        {1, 0, 0},        // wait until read_sem == 0
    };

    semop(global_msg.semid, ops, sizeof(ops) / sizeof(ops[0]));
}

void msg_write_signal(void)
{
    struct sembuf ops[] = {
        {0, -1, SEM_UNDO}, // write_sem -= 1
    };

    semop(global_msg.semid, ops, sizeof(ops) / sizeof(ops[0]));
}

void msg_read_wait(void)
{
    struct sembuf ops[] = {
        {0, 0, 0},        // wait until write_sem == 0
        {1, 1, SEM_UNDO}, // read_sem += 1
    };

    semop(global_msg.semid, ops, sizeof(ops) / sizeof(ops[0]));
}

void msg_read_signal(void)
{
    struct sembuf ops[] = {
        {1, -1, SEM_UNDO}, // read_sem -= 1
    };

    semop(global_msg.semid, ops, sizeof(ops) / sizeof(ops[0]));
}

void msg_tell(int sock, char *msg)
{
    net_write(sock, msg, strlen(msg));
}

void msg_prompt(int sock)
{
    msg_tell(sock, "% ");
}

void msg_motd(int sock)
{
    char msg[] = "****************************************\n"
                 "** Welcome to the information server. **\n"
                 "****************************************\n";

    msg_tell(sock, msg);
}