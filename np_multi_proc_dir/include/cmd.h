#ifndef CMD_H
#define CMD_H

#include <stdint.h>

#include "pidlist.h"

#define PIPE_ORDINARY   0x00000001
#define PIPE_NUM_STDOUT 0x00000002
#define PIPE_NUM_OUTERR 0x00000004
#define PIPE_FIL_STDOUT 0x00000008
#define PIPE_USR_STDOUT 0x00000010
#define PIPE_USR_STDIN  0x00000020

#define PIPE_NUM        (PIPE_NUM_STDOUT | PIPE_NUM_OUTERR)

typedef struct argv_node_tag argv_node;
struct argv_node_tag {
    argv_node *next;
    char *argv;
};

typedef struct cmd_node_tag cmd_node;
struct cmd_node_tag {
    // Next pipe command
    cmd_node *next;

    // Command
    char *cmd;
    char *cmd_line;

    // Arguments
    argv_node *argv;

    // Redirected output path
    char *rd_output;

    int cmd_len;
    int argv_len;

    // Pipe type
    // 0: None
    // 1: |   PIPE_ORDINARY   Pipe stdout
    // 2: |x  PIPE_NUM_STDOUT Pipe stdout
    // 3: !x  PIPE_NUM_OUTERR Pipe stdout and stderr
    // 4: > f PIPE_FIL_STDOUT Pipe stdout to file
    // 5: >x  PIPE_USR_STDOUT Pipe stdout to uid
    // 6: <x  PIPE_USR_STDIN  Pipe user pipe to stdin
    int pipetype;

    // Numbered Pipe
    int numbered; 

    // User Pipe
    uint32_t to_uid, from_uid; 
};

extern void cmd_init();

extern cmd_node* cmd_parse(char *cmd_line);

extern int cmd_run(cmd_node *cmd);

#endif