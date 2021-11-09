#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include "npshell.h"
#include "netio.h"
#include "sys_variable.h"
#include "prompt.h"
#include "cmd.h"

uint32_t global_uid; // current user uid
int global_sock;     // current user socket

static void npshell_init();

int npshell_run(uint32_t uid, int cs)
{
    char cmd_line[MAX_CMDLINE_LEN] = { 0 };
    int cmd_line_len;
    cmd_node *cmd;

    // Initialization
    global_uid = uid;
    global_sock = cs;
    npshell_init();

    // Main shell loop
    while (1) {
        // Outputing Prompt
        msg_prompt(cs);

        // Reading command
        cmd_line_len = net_readline(cs, cmd_line, MAX_CMDLINE_LEN);

        if (cmd_line_len == -1) {
            // Disconnect
            signal(SIGUSR1, SIG_IGN);
            return 1;
        } else if (!cmd_line_len) {
            // Empty command
            continue;
        }

        // Parsing command
        cmd = cmd_parse(cmd_line);

        if (cmd == (cmd_node*)-1) {
            // Exit
            signal(SIGUSR1, SIG_IGN);
            return 1;
        }

        // Debug
#ifdef DEBUG
        if (cmd) {
            for (cmd_node *c = cmd; c; c = c->next) {
                printf("cmd: %s\n", c->cmd);
                printf("argv_len: %d\n", c->argv_len);
                for (argv_node *argv = c->argv; argv; argv = argv->next) {
                    printf("\t%s\n", argv->argv);
                }
                if (c->rd_output)
                    printf("rd_output: %s\n", c->rd_output);
                if (c->pipetype)
                    printf("pipetype : %d\n", c->pipetype);
                if (c->numbered)
                    printf("numbered : %d\n", c->numbered);
            }
        }
#endif

        // Executing command
        cmd_run(cmd);
    }
}

static void npshell_init()
{
    // Initializing PATH
    setenv("PATH", "bin:.", 1);
    
    cmd_init();
}

