#include <stdio.h>
#include <stdlib.h>

#include "npshell.h"
#include "sys_variable.h"
#include "prompt.h"
#include "cmd.h"

// run single command
int npshell_run_single_command(user *user)
{
    char cmd_line[MAX_CMDLINE_LEN] = { 0 };
    int cmd_line_len;
    cmd_node *cmd;

    // Reading command
    cmd_line_len = cmd_read(user->sock, cmd_line);

    if (cmd_line_len == -1) {
        // Disconnect
        return 1;
    } else if (!cmd_line_len) {
        // Empty command
        // Outputing Prompt
        prompt(user->sock);

        return 0;
    }

    // Parsing command
    cmd = cmd_parse(user, cmd_line);

    if (cmd == (cmd_node*)-1) {
        // Exit
        return 1;
    }

    // Debug
#ifdef DEBUG
    if (cmd) {
        for (cmd_node *c = cmd; c; c = c->next) {
            printf("cmd     : %s\n", c->cmd);
            printf("cmd_line: %s\n", c->cmd_line);
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
            if (c->to_uid)
                printf("to_uid   : %d\n", c->to_uid);
            if (c->from_uid)
                printf("from_uid : %d\n", c->from_uid);
        }
    }
#endif

    // Executing command
    cmd_run(user, cmd);

    // Outputing Prompt
    prompt(user->sock);

    return 0;
}
