#ifndef USER_H
#define USER_H

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "unused_uid.h"

typedef struct user_tag user;
struct user_tag {
    uint32_t inused;
    pid_t    pid;    // process id
    uint16_t port;

    // semaphore for ruid
    // 0: write_sem
    // 1: read_sem
    // 2: c1_sem
    // 3: c2_sem
    // after write to ruid, must read ruid before next write to ruid
    // API: 
    //   user_sem_read_wait
    //   user_sem_read_signal
    //   user_sem_write_wait
    //   user_sem_write_signal
    int semid;       
    int ruid;        // remote uid

    char ip[0x10];   // xxx.xxx.xxx.xxx
    char name[0x20]; // maximum length is 20, 0x20 is for memory alignment
};

typedef struct user_manager_tag user_manager_struct;
struct user_manager_tag {
    int shmid; // shared memory id
    int semid; // semaphore id
    
    unused_uid unused_uid;

    // this user array resides in shared memory
    // the uid is array index
    user *all_users;
};

extern user_manager_struct user_manager;

extern void user_manager_init(void);

extern void user_manager_release(void);

extern void user_sem_read_wait(int semid);

extern void user_sem_read_signal(int semid);

extern void user_sem_write_wait(int semid);

extern void user_sem_write_signal(int semid);

extern uint32_t user_new(struct sockaddr_in caddr);

extern void user_release(uint32_t uid);

extern void user_pipe_open(void);

extern int user_pipe_from(uint32_t from_uid, char *cmd_line);

extern int user_pipe_to(uint32_t to_uid, char *cmd_line);

extern void user_pipe_release(uint32_t from_uid);

extern void user_broadcast(char *msg, int msg_type);

extern void user_cmd_who(void);

extern void user_cmd_tell(uint32_t uid, char *msg);

extern void user_cmd_yell(char *msg);

extern void user_cmd_name(char *name);

#endif