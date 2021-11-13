#include <stdio.h>
#include <stdlib.h>
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

    // global_msg.msg->type = 0;
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

    // printf("%d| msg_write_wait\n", global_uid);

    semop(global_msg.semid, ops, sizeof(ops) / sizeof(ops[0]));
}

static void msg_write_signal(void)
{
    struct sembuf ops[] = {
        {0, -1, SEM_UNDO}, // write_sem -= 1
    };

    // printf("%d| msg_write_signal\n", global_uid);

    semop(global_msg.semid, ops, sizeof(ops) / sizeof(ops[0]));
}

static void msg_read_wait(void)
{
    struct sembuf ops[] = {
        {0, 0, 0},        // wait until write_sem == 0
        {1, 1, SEM_UNDO}, // read_sem += 1
    };

    // printf("%d| msg_read_wait\n", global_uid);

    semop(global_msg.semid, ops, sizeof(ops) / sizeof(ops[0]));
}

static void msg_read_signal(void)
{
    struct sembuf ops[] = {
        {1, -1, SEM_UNDO}, // read_sem -= 1
    };

    // printf("%d| msg_read_signal\n", global_uid);

    semop(global_msg.semid, ops, sizeof(ops) / sizeof(ops[0]));
}

void msg_set_msg(char *msg_content, int msg_type, int parameter)
{
    char *msg = malloc(sizeof(char) * (strlen(msg_content) + 0x10));
    int len;
    int left_len;
    char *mem;
    int *pwo; // write_offset
    msg_str *ptr;

    // Build message header
    // Message:
    //   "<int type> <parameter 1> <parameter 2> ... <content><MSG_STR_DELIM>"
    //   e.g.
    //     "1 1 ***Logout Message***<MSG_STR_DELIM>"
    switch (msg_type)
    {
    case MSG_LOGOUT:
        // "MSG_LOGOUT <logout_uid> <content><MSG_STR_DELIM>"
        sprintf(msg, "%d %d %s%c", MSG_LOGOUT, global_uid, msg_content, MSG_STR_DELIM);
        break;
    
    case MSG_TELL:
        // "MSG_TELL <to_uid> <content><MSG_STR_DELIM>"
        sprintf(msg, "%d %d %s%c", MSG_TELL, parameter, msg_content, MSG_STR_DELIM);
        break;
    
    default:
        // "MSG_NONE <content><MSG_STR_DELIM>"
        sprintf(msg, "%d %s%c", MSG_NONE, msg_content, MSG_STR_DELIM);
        break;
    }

    len = strlen(msg);

    msg_write_wait();

    mem = global_msg.msg->mem;
    pwo = &(global_msg.msg->write_offset);
    left_len = MAX_MSG_LEN - *pwo - 1; // 1 for terminated NULL byte for ring buffer
    ptr = (msg_str *)&(mem[*pwo]);
    
    // Write message
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

    free(msg);
}

void msg_read_msg(void)
{
    char *mem;
    int wo; // write_offset
    int ro;
    int *pro; // read_offset
    int total_len = 0;
    char *content;
    msg_str *ptr;

    msg_read_wait();

    mem = global_msg.msg->mem;
    wo = global_msg.msg->write_offset;
    pro = &(global_msg.read_offset);
    ro = *pro;

    if ((unsigned char)mem[*pro] != MSG_STR_TAG) {
        // Lose some message
        // Reset read_offset
        *pro = wo;

        msg_read_signal();

        return;
    }

    // Get length
    ptr = (msg_str *)&(mem[*pro]);
    content = ptr->content;

    while (*pro != wo) {
        int len;

        len = strlen(content);

        total_len += len;

        *pro += len + 1; // 1 for NULL byte
        *pro %= MAX_MSG_LEN;
        if (*pro + 1 == MAX_MSG_LEN) 
            *pro = 0;

        content = &(mem[*pro]);
    }

    // Read message
    if (total_len) {
        char *buf = malloc(sizeof(char) * (total_len + 1));
        int idx = 0;
        int msg_type;
        int parameter;
        char *token;
        char *token_end;
        char delims[] = {MSG_STR_DELIM, 0};

        ptr = (msg_str *)&(mem[ro]);
        content = ptr->content;

        while (ro != wo) {
            int len;

            strcpy(&buf[idx], content);

            len = strlen(content);
            idx += len;

            ro += len + 1; // 1 for NULL byte
            ro %= MAX_MSG_LEN;
            if (ro + 1 == MAX_MSG_LEN) 
                ro = 0;

            content = &(mem[ro]);
        }

        printf("%d| buf: %s", global_uid, buf);

        msg_read_signal();
        
        token = strtok_r(buf, delims, &token_end);

        if (!token) {
            free(buf);
            return;
        }
        
        // Unpack message
        while (token) {
            char *token2;
            char *token2_end;
            
            printf("%d|\ttoken: %s", global_uid, token);
            printf("%d|\t- %d %d %d\n", global_uid, token[0], token[1], token[2]);

            token2 = strtok_r(token, " ", &token2_end);

            // "<int type> <parameter 1> <parameter 2> ... <content>"
            if (!token2) {
                free(buf);
                return;
            }

            if ((unsigned char)token2[0] == MSG_STR_TAG) {
                token2 += 1;
            }

            msg_type = atoi(token2);
            printf("%d|\t+ %d %d %d\n", global_uid, token2[0], token2[1], token2[2]);
            printf("%d|\tmsg_type: %d\n", global_uid, msg_type);

            switch (msg_type)
            {
            case MSG_LOGOUT:
                // "MSG_LOGOUT <logout_uid> <content>"
                token2 = strtok_r(NULL, " ", &token2_end);
                parameter = atoi(token2); // logout_uid
                user_pipe_release(parameter);

                token2 = token2 + strlen(token2) + 1;
                msg_tell(global_sock, token2);
                break;

            case MSG_TELL:
                // "MSG_TELL <to_uid> <content>"
                token2 = strtok_r(NULL, " ", &token2_end);
                parameter = atoi(token2); // to_uid

                if (parameter == global_uid) {
                    token2 = token2 + strlen(token2) + 1;
                    msg_tell(global_sock, token2);
                }
                break;

            default:
                // "MSG_NONE <content>"
                token2 = token2 + strlen(token2) + 1;
                msg_tell(global_sock, token2);
                break;
            }

            token = strtok_r(NULL, delims, &token_end);
        }

        free(buf);

        return;
    }

    msg_read_signal();

    return;
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