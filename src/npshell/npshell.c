#include <stdio.h>
#include <stdlib.h>

#include "npshell.h"
#include "sys_variable.h"
#include "prompt.h"
#include "cmd.h"

static void npshell_init(void);

int npshell_run(void)
{
    char cmd_line[MAX_CMDLINE_LEN] = { 0 };
    int cmd_line_len;
    cmd_node *cmd;

    // Initialization
    npshell_init();

    // Main shell loop
    while (1) {
        // Outputing Prompt
        prompt();

        // Reading command
        cmd_line_len = cmd_read(cmd_line);

        if (!cmd_line_len) {
            // Empty command
            continue;
        }

        // Parsing command
        cmd = cmd_parse(cmd_line);

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

static void npshell_init(void)
{
    // Initializing PATH
    setenv("PATH", "bin:.", 1);
    
    cmd_init();
}

