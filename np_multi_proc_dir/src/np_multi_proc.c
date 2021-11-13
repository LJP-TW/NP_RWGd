#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>

#include "npshell.h"
#include "user.h"
#include "prompt.h"
#include "pidlist.h"

static pid_list *child_plist;

static void np_multi_proc_init(void);

static void SIGINT_handler(int signum);
static void SIGINT_child_handler(int signum);
static void SIGCHLD_handler(int signum);

// ./np_multi_proc 12345 # Listen on 0.0.0.0:12345
int main(int argc, char **argv)
{
    int port;
    int ss; // server socket
    int enable = 1;
    struct sockaddr_in saddr;

    if (argc != 2) {
        // TODO: show help
        exit(1);
    }

    np_multi_proc_init();

    port = atoi(argv[1]);

    if ((ss = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        // TODO: handle error
        perror("Socket()");
        exit(1);
    }

    if (setsockopt(ss, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        exit(1);
    }

    saddr.sin_family = AF_INET;
    saddr.sin_port   = htons(port);
    saddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(ss, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
        // TODO: handle error
        perror("Bind()");
        exit(1);
    }

    if (listen(ss, 30) != 0) {
        // TODO: handle error
        perror("Listen()");
        exit(1);
    }

    while (1) {
        int cs; // client socket
        unsigned int caddrlen;
        pid_t pid;
        struct sockaddr_in caddr;

        caddrlen = sizeof(caddr);
        if ((cs = accept(ss, (struct sockaddr *)&caddr, &caddrlen)) == -1) {
            // TODO: handle error
            perror("Accept()");
            exit(1);
        }

        if ((pid = fork()) > 0) {
            // Parent process
            plist_insert_block(child_plist, pid);

            // Close unused fd
            close(cs);
        } else if (!pid) {
            // Child process
            char msgbuf[0x60] = { 0 };
            uint32_t uid;

            // New session
            if (setsid() < 0) {
                perror("setsid()");
                exit(errno);
            }

            signal(SIGINT, SIGINT_child_handler);

            // printf("[>] -- login\n");

            // New user
            uid = user_new(caddr);
            
            // printf("[>] %d login\n", uid);

            // Welcome message
            msg_motd(cs);

            // printf("[>] %d login motd\n", uid);

            // Broadcast login message
            sprintf(msgbuf, "*** User '%s' entered from %s:%d. ***\n", 
                user_manager.all_users[uid].name, 
                user_manager.all_users[uid].ip,
                user_manager.all_users[uid].port);

            user_broadcast(msgbuf, MSG_NONE);
            msg_tell(cs, msgbuf);

            // printf("[>] %d login user_broadcast & tell\n", uid);

            msg_reset_read_offset();

            // Close unused fd
            close(ss);

            // Run npshell
            npshell_run(uid, cs);

            // Broadcast logout message
            sprintf(msgbuf, "*** User '%s' left. ***\n", \
                user_manager.all_users[uid].name);

            // printf("[>] %d logout\n", uid);

            user_broadcast(msgbuf, MSG_LOGOUT);

            // printf("[>] %d logout broadcast\n", uid);

            user_release(uid);

            // printf("[>] %d logout release\n", uid);

            // Kill all child command
            kill(0, SIGKILL);

            exit(0);
        } else {
            // TODO: Handle fork error
            perror("[x] fork error\n");
        }
    }
}

static void np_multi_proc_init(void)
{
    signal(SIGINT, SIGINT_handler);
    signal(SIGCHLD, SIGCHLD_handler);
    signal(SIGUSR1, SIG_IGN);

    user_manager_init();
    global_msg_init();
    
    child_plist = plist_init();
}

static void SIGINT_handler(int signum)
{
    if (signum != SIGINT)
        return;

    // Exit

    // Close all shell
    for (pid_node *pn = child_plist->head; pn; pn = pn->next) {
        kill(pn->pid, SIGINT);
    }

    user_manager_release();
    global_msg_release();    

    // TODO: Close all FIFO

    // Bye
    exit(0);
}

static void SIGINT_child_handler(int signum)
{
    if (signum != SIGINT)
        return;

    user_release(global_uid);
    
    // Kill all child command
    kill(0, SIGKILL);

    // Bye
    // exit(0);
}

static void SIGCHLD_handler(int signum)
{
    int cpid;

    if (signum != SIGCHLD)
        return;

    cpid = wait(NULL);

    plist_delete_by_pid(child_plist, cpid);
}
