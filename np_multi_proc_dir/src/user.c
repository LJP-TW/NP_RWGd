#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sem.h>
#include <sys/shm.h>

#include "user.h"
#include "prompt.h"
#include "npshell.h"

#define MAX_USER_ID 30

user_manager_struct user_manager;

// user pipe fd
static int up_fds[MAX_USER_ID + 1];

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
        {0, -1, 0}, // sem[0] -= 1
    };

    semop(user_manager.semid, ops, sizeof(ops) / sizeof(ops[0]));
}

void user_manager_init(void)
{
    struct stat st = {0};

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

    // Create directory for user fifo pipe
    // user_pipe/<to_uid>/<from_uid>
    if (stat("user_pipe", &st) == -1) {
        mkdir("user_pipe", 0700);
    }

    for (int i = 1; i <= MAX_USER_ID; ++i) {
        char dirname[0x10] = { 0 };

        sprintf(dirname, "user_pipe/%d", i);

        if (stat(dirname, &st) == -1) {
            mkdir(dirname, 0700);
        }
    }
}

void user_manager_release(void)
{
    unused_uid_release(&(user_manager.unused_uid));
    semctl(user_manager.semid, 0, IPC_RMID);
    shmctl(user_manager.shmid, IPC_RMID, NULL);
}

void user_sem_read_wait(int semid)
{
    struct sembuf ops[] = {
        {1, 0, 0},        // wait until read_sem == 0
        {1, 1, SEM_UNDO}, // read_sem += 1
        {3, 0, 0},        // wait until c2_sem == 0
    };

    semop(semid, ops, sizeof(ops) / sizeof(ops[0]));
}

void user_sem_read_signal(int semid)
{
    struct sembuf ops[] = {
        {1, -1, 0}, // read_sem -= 1
        {2, -1, 0}, // c1_sem -= 1
        {3,  1, 0}, // c2_sem += 1
    };

    semop(semid, ops, sizeof(ops) / sizeof(ops[0]));
}

void user_sem_write_wait(int semid)
{
    struct sembuf ops[] = {
        {0, 0, 0},        // wait until write_sem == 0
        {0, 1, SEM_UNDO}, // write_sem += 1
        {2, 0, 0},        // wait until c1_sem == 0
    };

    semop(semid, ops, sizeof(ops) / sizeof(ops[0]));
}

void user_sem_write_signal(int semid)
{
    struct sembuf ops[] = {
        {0, -1, 0}, // write_sem -= 1
        {2,  1, 0}, // c1_sem += 1
        {3, -1, 0}, // c2_sem -= 1
    };

    semop(semid, ops, sizeof(ops) / sizeof(ops[0]));
}

static void user_sem_init(int semid)
{
    struct sembuf ops[] = {
        {3, 1, 0}, // c2_sem += 1
    };

    semop(semid, ops, sizeof(ops) / sizeof(ops[0]));
}

uint32_t user_new(struct sockaddr_in caddr)
{
    sem_wait();

    uint32_t uid = uid_alloc(&(user_manager.unused_uid));

    // TODO: Handle error if no free uid

    user_manager.all_users[uid].inused = 1;
    user_manager.all_users[uid].pid = getpid();
    user_manager.all_users[uid].port = ntohs(caddr.sin_port);
    user_manager.all_users[uid].semid = semget(IPC_PRIVATE, 4, IPC_CREAT | IPC_EXCL | 0600);
    user_sem_init(user_manager.all_users[uid].semid);
    user_manager.all_users[uid].ruid = 0;
    strcpy(user_manager.all_users[uid].ip, inet_ntoa(caddr.sin_addr));
    strcpy(user_manager.all_users[uid].name, "(no name)");

    user_manager.cnt += 1;

    sem_signal();

    return uid;
}

void user_release(uint32_t uid)
{
    DIR *d;
    struct dirent *dir;
    char buf[0x120] = { 0 };

    sprintf(buf, "user_pipe/%d", global_uid);

    d = opendir(buf);

    sem_wait();
    
    user_manager.all_users[uid].inused = 0;

    uid_release(&(user_manager.unused_uid), uid);
    semctl(user_manager.all_users[uid].semid, 0, IPC_RMID);

    user_manager.cnt -= 1;

    // Remove fifo pipe
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_type == DT_FIFO) {
                sprintf(buf, "user_pipe/%d/%s", global_uid, dir->d_name);
                unlink(buf);
            }
        }
        closedir(d);
    }

    sem_signal();
}

static uint32_t user_find_by_name(char *name)
{
    // Must acquire semaphore before call this function
    for (int i = 1; i <= MAX_USER_ID; ++i) {
        if (user_manager.all_users[i].inused) {
            if (!strcmp(user_manager.all_users[i].name, name)) {
                return i;
            }
        }
    }

    return 0;
}

static int open_null_fd(void)
{
    return open("/dev/null", O_RDWR);
}

static void user_msg_err_not_exists(uint32_t uid)
{
    char msgbuf[0x30];

    sprintf(msgbuf, 
            "*** Error: user #%d does not exist yet. ***\n",
            uid);

    msg_tell(global_sock, msgbuf);
}

static void user_msg_err_fifo_not_exists(uint32_t uid)
{
    char msgbuf[0x40];

    sprintf(msgbuf, 
            "*** Error: the pipe #%d->#%d does not exist yet. ***\n",
            uid,
            global_uid);

    msg_tell(global_sock, msgbuf);
}

static void user_msg_err_fifo_exists(uint32_t uid)
{
    char msgbuf[0x40];

    sprintf(msgbuf, 
            "*** Error: the pipe #%d->#%d already exists. ***\n",
            global_uid,
            uid);

    msg_tell(global_sock, msgbuf);
}

void user_pipe_open(void)
{
    // fifoname: user_pipe/<to_uid>/<from_uid>
    char fifoname[0x20];
    int from_uid;
    int fd;

    user_sem_read_wait(user_manager.all_users[global_uid].semid);

    from_uid = user_manager.all_users[global_uid].ruid;
    
    sprintf(fifoname, "user_pipe/%d/%d", global_uid, from_uid);

    if ((fd = open(fifoname, O_RDONLY)) < 0) {
        // TODO: Report error
        perror("[x] Server error: user_pipe_open");
    }

    if (up_fds[from_uid]) {
        close(up_fds[from_uid]);
    }

    up_fds[from_uid] = fd;

    user_sem_read_signal(user_manager.all_users[global_uid].semid);
}

int user_pipe_from(uint32_t from_uid, char *cmd_line)
{
    // fifoname: user_pipe/<to_uid>/<from_uid>
    char fifoname[0x20];
    struct stat st = {0};
    char *msgbuf;

    sem_wait();

    if (!user_manager.all_users[from_uid].inused) {
        sem_signal();

        user_msg_err_not_exists(from_uid);

        return open_null_fd();
    }
    
    // Making broadcast message
    msgbuf = malloc(sizeof(char) * strlen(cmd_line) + 0x80);
    sprintf(msgbuf, 
            "*** %s (#%d) just received from %s (#%d) by '%s' ***\n",
            user_manager.all_users[global_uid].name, 
            global_uid,
            user_manager.all_users[from_uid].name, 
            from_uid,
            cmd_line);

    sem_signal();

    sprintf(fifoname, "user_pipe/%d/%d", global_uid, from_uid);

    if (stat(fifoname, &st) == -1) {
        user_msg_err_fifo_not_exists(from_uid);

        free(msgbuf);

        return open_null_fd();
    }

    // Broadcast
    user_broadcast(msgbuf, MSG_NONE);

    free(msgbuf);

    return up_fds[from_uid];
}

int user_pipe_to(uint32_t to_uid, char *cmd_line)
{
    // fifoname: user_pipe/<to_uid>/<from_uid>
    char fifoname[0x20];
    char *msgbuf;
    pid_t to_pid;
    int fd;

    sem_wait();

    if (!user_manager.all_users[to_uid].inused) {
        sem_signal();

        user_msg_err_not_exists(to_uid);

        return open_null_fd();
    }
        
    to_pid = user_manager.all_users[to_uid].pid;
    sprintf(fifoname, "user_pipe/%d/%d", to_uid, global_uid);

    if ((mkfifo(fifoname, 0600)) < 0) {
        sem_signal();
        
        user_msg_err_fifo_exists(to_uid);

        return open_null_fd();
    }

    // Set shared memory, let to_uid user can know which pipe to read
    user_sem_write_wait(user_manager.all_users[to_uid].semid);

    user_manager.all_users[to_uid].ruid = global_uid;

    user_sem_write_signal(user_manager.all_users[to_uid].semid);

    // Making broadcast message
    msgbuf = malloc(sizeof(char) * strlen(cmd_line) + 0x80);
    sprintf(msgbuf, 
            "*** %s (#%d) just piped '%s' to %s (#%d) ***\n",
            user_manager.all_users[global_uid].name, 
            global_uid,
            cmd_line,
            user_manager.all_users[to_uid].name, 
            to_uid);

    sem_signal();

    // Send signal to notify to_uid to open read pipe
    kill(to_pid, SIGUSR2);

    if ((fd = open(fifoname, O_WRONLY)) < 0) {
        // TODO: Report bug
        perror("[x] Server error: open(fifoname, O_WRONLY)");

        free(msgbuf);

        return open_null_fd();
    }

    // Broadcast
    user_broadcast(msgbuf, MSG_NONE);

    free(msgbuf);

    return fd;
}

void user_pipe_release(uint32_t from_uid)
{
    // unlink fifo
    // fifoname: user_pipe/<to_uid>/<from_uid>
    char fifoname[0x20];

    sprintf(fifoname, "user_pipe/%d/%d", global_uid, from_uid);

    unlink(fifoname);

    if (up_fds[from_uid]) {
        close(up_fds[from_uid]);
        up_fds[from_uid] = 0;
    }
}

void user_broadcast(char *msg, int msg_type)
{
    // Must release user_manager semaphore before call this function

    msg_write_wait();

    // Write message
    global_msg.msg->type = msg_type;

    if (msg_type & MSG_LOGOUT) {
        global_msg.msg->uid = global_uid;
    }

    strncpy(global_msg.msg->content, msg, MAX_MSG_LEN);

    msg_write_signal();

    // Send signal to notify client to read message
    for (int i = 1; i < MAX_USER_ID; ++i) {
        sem_wait();

        if (user_manager.all_users[i].inused) {
            kill(user_manager.all_users[i].pid, SIGUSR1);
        }

        sem_signal();
    }
}

static void user_tell(uint32_t uid, char *msg)
{
    // Must release user_manager semaphore before call this function

    msg_write_wait();

    // Write message
    global_msg.msg->type = MSG_NONE;
    strncpy(global_msg.msg->content, msg, MAX_MSG_LEN);

    msg_write_signal();

    // Send signal to notify client to read message
    sem_wait();

    if (user_manager.all_users[uid].inused) {
        kill(user_manager.all_users[uid].pid, SIGUSR1);
    }

    sem_signal();
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

void user_cmd_tell(uint32_t uid, char *msg)
{
    char *buf = malloc(sizeof(char) * strlen(msg) + 0x40);
    
    sem_wait();

    if (user_manager.all_users[uid].inused) {
        sprintf(buf, "*** %s told you ***: %s\n", \
                user_manager.all_users[global_uid].name, \
                msg);

        sem_signal();

        user_tell(uid, buf);
    } else {
        sem_signal();

        sprintf(buf, "*** Error: user #%d does not exist yet. ***\n", uid);
        msg_tell(global_sock, buf);
    }

    free(buf);
}

void user_cmd_yell(char *msg)
{
    char *buf = malloc(sizeof(char) * strlen(msg) + 0x40);

    sem_wait();
    sprintf(buf, "*** %s yelled ***: %s\n", \
            user_manager.all_users[global_uid].name, msg);
    sem_signal();

    user_broadcast(buf, MSG_NONE);

    free(buf);
}

void user_cmd_name(char *name)
{
    char buf[0x70] = { 0 };
    
    sem_wait();

    uint32_t uid = user_find_by_name(name);

    if (uid) {
        sem_signal();

        sprintf(buf, "*** User '%s' already exists. ***\n", name);
        msg_tell(global_sock, buf);
    } else {
        strcpy(user_manager.all_users[global_uid].name, name);

        // e.g. *** User from 140.113.215.62:1201 is named 'Mike'. ***
        sprintf(buf, "*** User from %s:%d is named '%s'. ***\n", \
                user_manager.all_users[global_uid].ip, \
                user_manager.all_users[global_uid].port, \
                name);

        sem_signal();

        user_broadcast(buf, MSG_NONE);
    }
}