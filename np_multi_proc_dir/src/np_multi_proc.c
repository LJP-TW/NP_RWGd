#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>

#include "npshell.h"
#include "user.h"
#include "prompt.h"

static void np_multi_proc_init(void);

static void SIGINT_handler(int signum);

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

            // Close unused fd
            close(cs);
        } else if (!pid) {
            // Child process
            char msgbuf[0x60] = { 0 };
            uint32_t uid;

            // New user
            uid = user_new(caddr);

            printf("uid: %d\n", uid);

            // Welcome message
            msg_motd(cs);

            // Broadcast login message
            sprintf(msgbuf, "*** User '%s' entered from %s:%d. ***\n", 
                user_manager.all_users[uid].name, 
                user_manager.all_users[uid].ip,
                user_manager.all_users[uid].port);

            user_broadcast(msgbuf);
            msg_tell(cs, msgbuf);

            // Close unused fd
            close(ss);

            // Run npshell
            npshell_run(uid, cs);

            // Broadcast logout message
            sprintf(msgbuf, "*** User '%s' left. ***\n", \
                user_manager.all_users[uid].name);

            user_broadcast(msgbuf);

            user_release(uid);

            exit(0);
        } else {
            // TODO: Handle fork error
        }
    }
}

static void np_multi_proc_init(void)
{
    signal(SIGINT, SIGINT_handler);
    signal(SIGUSR1, SIG_IGN);

    user_manager_init();
    global_msg_init();
}

static void SIGINT_handler(int signum)
{
    if (signum != SIGINT)
        return;

    // Exit

    user_manager_release();
    global_msg_release();    

    // Close all shell
    kill(0, SIGKILL);

    // TODO: Close all FIFO

    // Bye
    exit(0);
}