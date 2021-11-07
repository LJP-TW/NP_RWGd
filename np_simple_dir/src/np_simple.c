#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>

#include "npshell.h"

// ./np_simple 12345 # Listen on 0.0.0.0:12345
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

            // Wait for child
            wait(NULL);
        } else if (!pid) {
            // Child process

            // Close unused fd
            close(ss);

            // Redirect fd
            // dup2(cs, STDIN_FILENO);
            // dup2(cs, STDOUT_FILENO);
            // dup2(cs, STDERR_FILENO);

            // Set buffer mode to NOBUF
            // setvbuf(stdin, NULL, _IONBF, 0);
            // setvbuf(stdout, NULL, _IONBF, 0);
            // setvbuf(stderr, NULL, _IONBF, 0);

            // Run npshell
            npshell_run(cs);

            exit(0);
        } else {
            // TODO: Handle fork error
        }
    }
}
