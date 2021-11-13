#include <unistd.h>
#include <string.h>
#include <sys/sem.h>
#include <sys/shm.h>

#include "prompt.h"
#include "user.h"
#include "netio.h"
#include "npshell.h"

message_buf global_msg;

void global_msg_init(void)
{
    global_msg.shmid = shmget(IPC_PRIVATE, sizeof(message), \
                              IPC_CREAT | IPC_EXCL | 0600);
    
    global_msg.msg = shmat(global_msg.shmid, NULL, 0);

    global_msg.msg->type = 0;
    global_msg.msg->write_offset = 0;
    global_msg.msg->mem[0] = 0;

    global_msg.read_offset = 0;

    // Semaphore of user_manager
    global_msg.semid = semget(IPC_PRIVATE, 2, IPC_CREAT | IPC_EXCL | 0600);
}

void global_msg_release(void)
{
    semctl(global_msg.semid, 0, IPC_RMID);
    shmctl(global_msg.shmid, IPC_RMID, NULL);
}

static void msg_write_wait(void)
{
    struct sembuf ops[] = {
        {0, 0, 0},        // wait until write_sem == 0
        {0, 1, SEM_UNDO}, // write_sem += 1
        {1, 0, 0},        // wait until read_sem == 0
    };

    semop(global_msg.semid, ops, sizeof(ops) / sizeof(ops[0]));
}

static void msg_write_signal(void)
{
    struct sembuf ops[] = {
        {0, -1, 0}, // write_sem -= 1
    };

    semop(global_msg.semid, ops, sizeof(ops) / sizeof(ops[0]));
}

static void msg_read_wait(void)
{
    struct sembuf ops[] = {
        {0, 0, 0},        // wait until write_sem == 0
        {1, 1, SEM_UNDO}, // read_sem += 1
    };

    semop(global_msg.semid, ops, sizeof(ops) / sizeof(ops[0]));
}

static void msg_read_signal(void)
{
    struct sembuf ops[] = {
        {1, -1, 0}, // read_sem -= 1
    };

    semop(global_msg.semid, ops, sizeof(ops) / sizeof(ops[0]));
}

void msg_set_msg(char *msg, int msg_type)
{
    int len = strlen(msg);
    int left_len;
    char *mem;
    int *pwo; // write_offset
    msg_str *ptr;

    msg_write_wait();

    mem = global_msg.msg->mem;
    pwo = &(global_msg.msg->write_offset);
    left_len = MAX_MSG_LEN - *pwo - 1; // 1 for terminated NULL byte for ring buffer
    ptr = (msg_str *)&(mem[*pwo]);
    
    // Write message
    global_msg.msg->type = msg_type;

    if (msg_type & MSG_LOGOUT) {
        global_msg.msg->uid = global_uid;
    }

    // 1 for terminated NULL byte
    // 1 for msg_str tag
    if (left_len >= len + 1 + 1) {
        ptr->tag = MSG_STR_TAG;
        strncpy(ptr->content, msg, len);
        ptr->content[len] = 0;

        *pwo += len + 1 + 1; // 1 for terminated NULL byte, 1 for msg_str tag
        *pwo %= (MAX_MSG_LEN - 1);
    } else {
        int last = left_len - 1; // -1 for msg_str tag
        ptr->tag = MSG_STR_TAG;
        strncpy(ptr->content, msg, last); 

        if (len != last) {
            strncpy(mem, &msg[last], len - last);
            mem[len - last] = 0;

            *pwo = len - last + 1;
        } else {
            *pwo = 0;
        }
    }

    msg_write_signal();
}

void msg_read_msg(void)
{
    char *mem;
    int wo; // write_offset
    int *pro; // read_offset

    msg_read_wait();

    mem = global_msg.msg->mem;
    wo = global_msg.msg->write_offset;
    pro = &(global_msg.read_offset);

    if (global_msg.msg->type & MSG_LOGOUT) {
        user_pipe_release(global_msg.msg->uid);
    }

    if ((unsigned char)mem[*pro] != MSG_STR_TAG) {
        // Lose some message
        // Reset read_offset
        *pro = wo;

        msg_read_signal();

        return;
    }

    while (*pro != wo) {
        int len;
        msg_str *ptr;

        ptr = (msg_str *)&(mem[*pro]);

        msg_tell(global_sock, ptr->content);

        len = strlen(ptr->content);

        *pro += (1 + len + 1); // 1 for msg_str tag, 1 for NULL byte
        *pro %= MAX_MSG_LEN;
        if (*pro + 1 == MAX_MSG_LEN) 
            *pro = 0;
    }
        
    msg_read_signal();
}

void msg_reset_read_offset(void)
{
    global_msg.read_offset = global_msg.msg->write_offset;
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