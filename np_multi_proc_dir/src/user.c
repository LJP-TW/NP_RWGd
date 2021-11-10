#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/sem.h>
#include <sys/shm.h>

#include "user.h"
#include "prompt.h"
#include "npshell.h"

#define MAX_USER_ID 30

user_manager_struct user_manager;

static void sem_wait(void)
{
    struct sembuf ops[] = {
        {0, 0, 0}, // wait until sem[0] == 0
        {0, 1, SEM_UNDO}, // sem[0] += 1
    };

    semop(user_manager.semid, ops, sizeof(ops) / sizeof(ops[0]));
}

static void sem_signal(void)
{
    struct sembuf ops[] = {
        {0, -1, SEM_UNDO}, // sem[0] -= 1
    };

    semop(user_manager.semid, ops, sizeof(ops) / sizeof(ops[0]));
}

void user_manager_init(void)
{
    // all_users    
    user_manager.shmid = shmget(IPC_PRIVATE, 
                                sizeof(user) * (MAX_USER_ID + 1), \
                                IPC_CREAT | IPC_EXCL | 0600);

    // Semaphore of user_manager
    user_manager.semid = semget(IPC_PRIVATE, 1, IPC_CREAT | IPC_EXCL | 0600);

    unused_uid_init(&(user_manager.unused_uid));
    
    // Init all_users array
    user_manager.all_users = shmat(user_manager.shmid, NULL, 0);

    for (int i = 1; i <= MAX_USER_ID; ++i) {
        user_manager.all_users[i].inused = 0;
    }

    user_manager.cnt = 0;
}

void user_manager_release(void)
{
    unused_uid_release(&(user_manager.unused_uid));
    semctl(user_manager.semid, 0, IPC_RMID);
    shmctl(user_manager.shmid, IPC_RMID, NULL);
}

uint32_t user_new(struct sockaddr_in caddr)
{
    sem_wait();

    uint32_t uid = uid_alloc(&(user_manager.unused_uid));

    // TODO: Handle error if no free uid

    user_manager.all_users[uid].inused = 1;
    user_manager.all_users[uid].pid = getpid();
    user_manager.all_users[uid].port = ntohs(caddr.sin_port);
    strcpy(user_manager.all_users[uid].ip, inet_ntoa(caddr.sin_addr));
    strcpy(user_manager.all_users[uid].name, "(no name)");

    user_manager.cnt += 1;

    sem_signal();

    return uid;
}

void user_release(uint32_t uid)
{
    sem_wait();
    
    user_manager.all_users[uid].inused = 0;

    uid_release(&(user_manager.unused_uid), uid);

    user_manager.cnt -= 1;

    sem_signal();
}

uint32_t user_find_by_name(char *name)
{
    sem_wait();

    for (int i = 1; i <= MAX_USER_ID; ++i) {
        if (user_manager.all_users[i].inused) {
            if (!strcmp(user_manager.all_users[i].name, name)) {
                sem_signal();

                return i;
            }
        }
    }

    sem_signal();

    return 0;
}

void user_broadcast(char *msg)
{
    msg_write_wait();

    // Write message
    strncpy(global_msg.msg, msg, MAX_MSG_LEN);

    msg_write_signal();

    // Send signal to notify client to read message
    for (int i = 1; i < MAX_USER_ID; ++i) {
        if (user_manager.all_users[i].inused) {
            kill(user_manager.all_users[i].pid, SIGUSR1);
        }
    }
}

void user_cmd_who(void)
{
    char buf[0x30] = { 0 };

    msg_tell(global_sock, "<ID>\t<nickname>\t<IP:port>\t<indicate me>\n");

    sem_wait();

    for (int i = 1; i < MAX_USER_ID; ++i) {
        if (user_manager.all_users[i].inused) {
            // ID
            sprintf(buf, "%d\t", i);
            msg_tell(global_sock, buf);

            // Nickname
            sprintf(buf, "%s\t", user_manager.all_users[i].name);
            msg_tell(global_sock, buf);

            // IP:Port
            sprintf(buf, "%s:%d\t", \
                    user_manager.all_users[i].ip, \
                    user_manager.all_users[i].port);
            msg_tell(global_sock, buf);

            // Is me?
            if (i == global_uid) {
                msg_tell(global_sock, "<-me");
            }
        
            msg_tell(global_sock, "\n");
        }
    }

    sem_signal();
}

void user_cmd_name(char *name)
{
    char buf[0x50] = { 0 };
    uint32_t uid = user_find_by_name(name);

    if (uid) {
        sprintf(buf, "*** User '%s' already exists. ***\n", name);
        msg_tell(global_sock, buf);
    } else {
        sem_wait();
        strcpy(user_manager.all_users[global_uid].name, name);

        // e.g. *** User from 140.113.215.62:1201 is named 'Mike'. ***
        sprintf(buf, "*** User from %s:%d is named '", \
                user_manager.all_users[global_uid].ip, \
                user_manager.all_users[global_uid].port);
        sem_signal();

        user_broadcast(buf);
        user_broadcast(name);
        user_broadcast("'. ***\n");
    }
}