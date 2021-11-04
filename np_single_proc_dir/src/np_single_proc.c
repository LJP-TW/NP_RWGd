#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <arpa/inet.h>

#include "prompt.h"
#include "npshell.h"
#include "netio.h"
#include "user.h"
#include "cmd.h"

#ifndef FD_COPY
#define FD_COPY(dest, src) memcpy((dest), (src), sizeof *(dest))
#endif

static void signal_handler(int signum)
{
    switch (signum) {
    case SIGPIPE:
        printf("SIGPIPE!\n");

        break;
    default:
        break;
    }
}

static void init(void)
{
    // TODO: Not sure about that.
    signal(SIGPIPE, signal_handler);

    user_list_init();
    
    cmd_init();
}

// ./np_single_proc 12345 # Listen on 0.0.0.0:12345
int main(int argc, char **argv)
{
    int port;
    int ss; // server socket
    int enable = 1;
    int nfds;
    struct sockaddr_in saddr;
    fd_set rfds;
    fd_set afds;

    // Initialize global variable 
    init();

    if (argc != 2) {
        // TODO: show help
        exit(1);
    }

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

    // Initialize fd set
    nfds = getdtablesize();
    FD_ZERO(&afds);
    FD_SET(ss, &afds);

    while (1) {
        FD_COPY(&rfds, &afds);

        // Select
        while (select(nfds, &rfds, (fd_set *)0, (fd_set *)0, (struct timeval *)0) < 0);

        // Server socket
        if (FD_ISSET(ss, &rfds)) {
            int cs; // client socket
            unsigned int caddrlen;
            struct sockaddr_in caddr;
            user *user;

            caddrlen = sizeof(caddr);
            if ((cs = accept(ss, (struct sockaddr *)&caddr, &caddrlen)) == -1) {
                // TODO: handle error
                perror("Accept()");
                exit(1);
            }

            // Initialize user struct
            user = user_init(caddr, cs);
            user_list_insert(user);

            // Update fd set
            FD_SET(cs, &afds);

            // Outputing Prompt
            prompt(cs);
        }

        // Client socket
        for (int fd = 0; fd < nfds; ++fd) {
            if (fd != ss && FD_ISSET(fd, &rfds)) {
                user *user;
                int ret;
                
                if (!(user = user_list_find_by_sock(fd))) {
                    // TODO: report error
                    perror("user_list_find_by_sock()");
                    exit(1);
                }
                
                // TODO: Fix client cannot recv first prompt
                // TODO: Fix when client send space first, server will hang

                ret = npshell_run_single_command(user);

                if (ret) {
                    // Disconnect
                    user_release(user);

                    // Update fd set
                    FD_CLR(fd, &afds);

                    // Close connection
                    close(fd);
                }
            }
        }
    }
}
