#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>

#include "sys_variable.h"
#include "cmd.h"
#include "pidlist.h"
#include "netio.h"
#include "nplist.h"
#include "prompt.h"
#include "npshell.h"
#include "user.h"

#define ASCII_SPACE 0x20

#define ARR_LEN(x) (sizeof(x)/sizeof(x[0]))

#define IS_ALPHABET(x) ((65 <= x && x <= 90) || (97 <= x && x <= 122))
#define IS_DIGIT(x) (48 <= x && x <= 57)
#define IS_ALPHADIGIT(x) (IS_ALPHABET(x) || IS_DIGIT(x))

// declared in unistd.h
extern char** environ;

const char *bulitin_cmds[] = {"setenv", 
                              "printenv", 
                              "exit",
                              "who",
                              "tell",
                              "yell",
                              "name",};

const char *special_symbols[] = {">", 
                                 "|",
                                 "!"};

static np_list *global_nplist;
static int use_sh_wait;
static pid_list *plist;
sigset_t sigset_SIGCHLD;

static void signal_handler(int signum)
{
    int cpid;
    int ok = 0;

    switch (signum) {
    case SIGCHLD:
        if (!use_sh_wait)
            return;

        // When child process ends, call signal_handler and wait
        cpid = wait(NULL);

        // Save to update plist
        ok = plist_delete_by_pid(plist, cpid);

        if (!ok) {
            plist_insert(sh_closed_plist, cpid);
        }

        break;
    case SIGUSR1:
        // Broadcast message
        msg_read_wait();

        msg_tell(global_sock, global_msg.msg);

        msg_read_signal();
        break;
    default:
        break;
    }
}

// Disable signal handler
static void disable_sh()
{
    sigset_t oldset;
    sigprocmask(SIG_BLOCK, &sigset_SIGCHLD, &oldset);

    use_sh_wait = 0;

    sigprocmask(SIG_SETMASK, &oldset, NULL);
}

// Enable signal handler
static void enable_sh()
{
    sigset_t oldset;
    sigprocmask(SIG_BLOCK, &sigset_SIGCHLD, &oldset);

    use_sh_wait = 1;

    sigprocmask(SIG_SETMASK, &oldset, NULL);
}

void cmd_init()
{
    // Register signal handler
    signal(SIGCHLD, signal_handler);
    signal(SIGUSR1, signal_handler);

    // Init pid_list
    closed_plist    = plist_init();
    sh_closed_plist = plist_init();
    alive_plist     = plist_init();

    // Init nplist
    global_nplist   = nplist_init();

    // Init sigset
    sigemptyset(&sigset_SIGCHLD);
    sigaddset(&sigset_SIGCHLD, SIGCHLD);
}

static cmd_node* cmd_node_init()
{
    cmd_node *node = malloc(sizeof(cmd_node));

    node->next = NULL;
    node->cmd  = NULL;
    node->argv = NULL;
    node->rd_output = NULL;
    node->cmd_len = 0;
    node->argv_len = 0;
    node->pipetype = 0;
    node->numbered = 0;

    return node;
}

static void cmd_node_release(cmd_node *cmd)
{
    argv_node *cur_an, *next_an;

    if (cmd->cmd)
        free(cmd->cmd);
    for (cur_an = cmd->argv; cur_an; cur_an = next_an) {
        next_an = cur_an->next;
        free(cur_an->argv);
        free(cur_an);
    }
    if (cmd->rd_output)
        free(cmd->rd_output);
    free(cmd);
}

static int valid_filepath(char *filepath)
{
    int ok = 1;

    for (char c = *filepath++; c; c = *filepath++) {
        if (IS_ALPHADIGIT(c)) {
            continue;
        }

        if (c == '.' || c == '/' || c == '_') {
            continue;
        }

        ok = 0;
        return ok;
    }

    return ok;
}

static int cmd_parse_special_symbols(cmd_node *cmd, char **token_ptr, int ssidx)
{
    // Parse special symbols
    char *token = *token_ptr;
    int number;

    switch (ssidx)
    {
    case 0:
        // >
        // Stdout redirection (cmd > file)
        if ((token = strtok(NULL, " ")) != NULL) {
            cmd->rd_output = strdup(token);
            cmd->pipetype = PIPE_FIL_STDOUT;
            *token_ptr = token;
        } else {
            // TODO: Report error
        }
        break;
    
    case 1:
        // |
        if (token[1] == 0) {
            // Ordinary pipe
            // cmd1 | cmd2
            cmd->pipetype = PIPE_ORDINARY;
        } else {
            // Numbered pipe
            // cmd1 |2
            number = atoi(&token[1]);
            if (number == 0) {
                // TODO: Report error
            } else {
                cmd->pipetype = PIPE_NUM_STDOUT;
                cmd->numbered = number;
            }
        }
        break;

    case 2:
        // !
        // Numbered pipe
        // cmd !2
        number = atoi(&token[1]);
        if (number == 0) {
            // TODO: Report error
        } else {
            cmd->pipetype = PIPE_NUM_OUTERR;
            cmd->numbered = number;
        }
        break;
    
    default:
        break;
    }

    return 0;
}

static int cmd_parse_bulitin_cmd(cmd_node *cmd, char *token, int bulitin_cmd_id)
{
    // Parse bulit-in command
    char *var;
    char *value;
    char *envvalue;

    switch (bulitin_cmd_id) {
    case 0:
        // setenv

        // TODO: Handle error

        var = strtok(NULL, " ");
        value = strtok(NULL, " ");
        setenv(var, value, 1);
        break;
    case 1:
        // printenv

        // TODO: Handle error

        var = strtok(NULL, " ");

        envvalue = getenv(var);
        if (envvalue) {
            char buf[0x100] = { 0 };
            sprintf(buf, "%s\n", envvalue);
            net_write(global_sock, buf, strlen(buf));
        }
        break;
    case 2:
        // exit
        return -1;
        break;
    case 3:
        // who
        user_cmd_who();
        break;
    case 4:
        // tell
        break;
    case 5:
        // yell
        break;
    case 6:
        // name
        break;
    }

    return 0;
}

cmd_node* cmd_parse(char *cmd_line)
{
    int firstcmd = 1;
    int bulitin_cmd_id = -1;
    char c;
    int cmd_len = 0;
    char *strtok_arg1 = cmd_line;
    char *token;
    cmd_node *cmd_head;
    cmd_node *cmd;
    cmd_node **curcmd = &cmd_head;
    argv_node *argv;

    // Parse command
    while ((token = strtok(strtok_arg1, " ")) != NULL) {
        argv_node **ptr;
        int ssidx; // Special symbol idx

        *curcmd = cmd_node_init();
        cmd = *curcmd;
        curcmd = &(cmd->next);

        // Check command is a valid path
        if (!valid_filepath(token)) {
            // TODO: Report error
            break;
        }

        // Check whether the command is built-in command
        if (firstcmd) {
            firstcmd = 0;
            strtok_arg1 = NULL;

            for (int i = 0; i < ARR_LEN(bulitin_cmds); ++i) {
                if (!strcmp(bulitin_cmds[i], token)) {
                    bulitin_cmd_id = i;
                    break;
                }
            }

            if (bulitin_cmd_id != -1) {
                int ret;
                
                ret = cmd_parse_bulitin_cmd(cmd, token, bulitin_cmd_id);

                if (ret == -1) {
                    return (cmd_node*)-1;
                }

                return NULL;
            }
        }
        
        // Ok, save this command
        cmd->cmd = strdup(token);
        cmd_len += 1;

        // Parse argv
        ptr = &(cmd->argv); 
        ssidx = -1;
        while ((token = strtok(NULL, " ")) != NULL) {
            c = token[0];

            for (int i = 0; i < ARR_LEN(special_symbols); ++i) {
                if (special_symbols[i][0] == c) {
                    // End of argv
                    ssidx = i;
                    break;
                }
            }

            if (ssidx != -1) {
                // End of argv
                break;
            }

            // Ok, save this argv
            argv = malloc(sizeof(argv_node));
            argv->next = NULL;
            argv->argv = strdup(token);

            *ptr = argv;
            ptr = &(argv->next);

            cmd->argv_len += 1;
        }

        // Parse special symbol
        if (token == NULL) {
            break;
        }

        cmd_parse_special_symbols(cmd, &token, ssidx);
    }

    cmd_head->cmd_len = cmd_len;

    return cmd_head;
}

int cmd_run(cmd_node *cmd)
{
    pid_t pid;
    int read_pipe = -1;
    np_node *np_out = NULL;
    cmd_node *next_cmd;
    np_node *np_in, *origin_np_in;

    plist = plist_init();

    // Enable signal handler
    enable_sh();

    // Handle numbered pipe
    nplist_update(global_nplist);
    origin_np_in = np_in = nplist_find_by_numbered(global_nplist, 0);

    while (cmd) {
        int cur_pipe[2] = {-1, -1};
        int filefd = -1;

        next_cmd = cmd->next;

        // Handle pipe
        switch(cmd->pipetype) {
        case PIPE_ORDINARY:
            if (pipe(cur_pipe)) {
                printf("[x] pipe error: %d\n", errno);
            }
            break;
        case PIPE_NUM_STDOUT:
        case PIPE_NUM_OUTERR:
            if (cmd->numbered) {
                np_out = nplist_find_by_numbered(global_nplist, cmd->numbered);
                if (!np_out) {
                    np_out = nplist_insert(global_nplist, cmd->numbered);
                }
            }
            break;
        case PIPE_FIL_STDOUT:
            filefd = open(cmd->rd_output, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU | S_IRGRP | S_IROTH);
            break;
        default:
            // No pipe
            break;
        }

        // Execute command
        if ((pid = fork()) > 0) {
            // Parent process

            // Handle pid list
            plist_insert_block(plist, pid);

            // Handle input pipe
            if (read_pipe != -1) {
                close(read_pipe); // read_pipe will be updated later
            }

            if (cur_pipe[1] != -1) {
                close(cur_pipe[1]);
                read_pipe = cur_pipe[0];
            } else {
                read_pipe = -1;
            }

            // Handle numbered pipe
            np_in = NULL;

            // Handle file pipe
            if (filefd != -1) {
                close(filefd);
            }

            // Free memory
            cmd_node_release(cmd);

            // Go to next command
            cmd = next_cmd;
        } else if (!pid) {
            // Child process
            char **argv;
            int idx;

            // Handle input pipe
            
            dup2(global_sock, STDIN_FILENO);

            if (np_in) {
                dup2(np_in->fd[0], STDIN_FILENO);
            } else if (read_pipe != -1) {
                dup2(read_pipe, STDIN_FILENO);
            }
            
            // Handle output pipe

            dup2(global_sock, STDOUT_FILENO);
            dup2(global_sock, STDERR_FILENO);

            switch(cmd->pipetype) {
            case PIPE_ORDINARY:
                close(cur_pipe[0]);
                dup2(cur_pipe[1], STDOUT_FILENO);
                break;
            case PIPE_NUM_STDOUT:
                close(np_out->fd[0]);
                dup2(np_out->fd[1], STDOUT_FILENO);
                break;
            case PIPE_NUM_OUTERR:
                close(np_out->fd[0]);
                dup2(np_out->fd[1], STDOUT_FILENO);
                dup2(np_out->fd[1], STDERR_FILENO);
                break;
            case PIPE_FIL_STDOUT:
                dup2(filefd, STDOUT_FILENO);
            default:
                // No pipe
                break;
            }

            // Close pipe
            for (int i = 3; i < 1024; ++i) {
                close(i);
            }

            // Make argv
            argv = malloc(sizeof(char *) * (cmd->argv_len + 2));
            argv[0] = cmd->cmd;
            idx = 1;
            for (argv_node *an = cmd->argv; an; an = an->next) {
                argv[idx++] = an->argv;
            }
            argv[idx] = NULL;

            // Execute command
            execvp(cmd->cmd, argv);

            // Handle error
            fprintf(stderr, "Unknown command: [%s].\n", cmd->cmd);
            exit(errno);
        } else {
            // Handle error
            pid_t cpid;

            // Disable signal handler
            disable_sh();

            // Wait for one process and re-run again
            cpid = wait(NULL);

            // Record closed pid
            plist_insert(closed_plist, cpid);
            plist_delete_intersect(plist, closed_plist);

            // Recycle all the resources
            switch(cmd->pipetype) {
            case PIPE_ORDINARY:
                if (cur_pipe[0] != -1) {
                    close(cur_pipe[0]);
                    close(cur_pipe[1]);
                }
                break;
            case PIPE_NUM_STDOUT:
            case PIPE_NUM_OUTERR:
                if (cmd->numbered) {
                    nplist_remove_by_numbered(global_nplist, cmd->numbered);
                }
                break;
            case PIPE_FIL_STDOUT:
                if (filefd != -1)
                    close(filefd);
                break;
            default:
                // No pipe
                break;
            }

            enable_sh();

            // Re-run
        }
    }

    // Disable wait in signal handler
    disable_sh();

    // close fd
    if (origin_np_in) {
        close(origin_np_in->fd[1]);
        origin_np_in->fd[1] = -1;
    }

    // Update pid list
    plist_merge(closed_plist, sh_closed_plist);
    plist_delete_intersect(alive_plist, closed_plist);

    if (!np_out) {
        int status;

        // Wait for origin_np_in
        if (origin_np_in) {
            plist_delete_intersect(origin_np_in->plist, closed_plist);
            for (pid_node *pn = origin_np_in->plist->head; pn; pn = pn->next) {
                waitpid(pn->pid, &status, 0);
            }
        }

        // Wait for plist
        plist_delete_intersect(plist, closed_plist);
        for (pid_node *pn = plist->head; pn; pn = pn->next) {
            waitpid(pn->pid, &status, 0);
        }

        // Free plist
        plist_release(plist);
    } else {
        // If there is origin_np_in, merge origin_np_in to plist
        if (origin_np_in) {
            plist_merge(plist, origin_np_in->plist);
        }

        // Update np_out->plist    
        if (!(np_out->plist)) {
            np_out->plist = plist;
        } else {
            plist_merge(np_out->plist, plist);
            plist_release(plist);
        }
    }

    enable_sh();

    return 0;
}
