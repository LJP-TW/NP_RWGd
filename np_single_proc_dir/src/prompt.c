#include <stdio.h>
#include <string.h>

#include "prompt.h"
#include "netio.h"
#include "user.h"

void prompt(int sock)
{
    msg_tell(sock, "% ");
}

void msg_tell(int sock, char *msg)
{
    net_write(sock, msg, strlen(msg));
}

void msg_motd(int sock)
{
    char msg[] = "***************************************\n"
                 "** Welcome to the information server **\n"
                 "***************************************\n";

    msg_tell(sock, msg);
}

void msg_broadcast(char *msg)
{
    user_node *cur = all_users->head;

    while (cur) {
        if (cur->user->sock != -1) {
            msg_tell(cur->user->sock, msg);
        }

        cur = cur->next;
    }
}

void msg_broadcast_login(user_node *node)
{
    char msgbuf[0x60] = { 0 };

    sprintf(msgbuf, "*** User '%s' entered from %s:%d. ***\n", 
                node->user->name, 
                node->user->ip,
                node->user->port);

    msg_broadcast(msgbuf);
}

void msg_broadcast_logout(user_node *node)
{
    char msgbuf[0x50] = { 0 };

    sprintf(msgbuf, "*** User '%s' left. ***\n", node->user->name);

    msg_broadcast(msgbuf);
}

void msg_broadcast_up_recv(char *cmd_line, user *from_user, user *to_user)
{
    char msgbuf[0x180];

    sprintf(msgbuf, 
            "*** %s (#%d) just received from %s (#%d) by '%s' ***\n",
            to_user->name, to_user->uid,
            from_user->name, from_user->uid,
            cmd_line);

    msg_broadcast(msgbuf);
}

void msg_broadcast_up_send(char *cmd_line, user *from_user, user *to_user)
{
    char msgbuf[0x180];

    sprintf(msgbuf, 
            "*** %s (#%d) just piped '%s' to %s (#%d) ***\n",
            from_user->name, from_user->uid,
            cmd_line,
            to_user->name, to_user->uid);

    msg_broadcast(msgbuf);
}

void msg_err_up_exists(int sock, uint32_t from_uid, uint32_t to_uid)
{
    char msgbuf[0x40];

    sprintf(msgbuf, 
            "*** Error: the pipe #%d->#%d already exists. ***\n",
            from_uid,
            to_uid);

    msg_tell(sock, msgbuf);
}

void msg_err_up_not_exists(int sock, uint32_t from_uid, uint32_t to_uid)
{
    char msgbuf[0x40];

    sprintf(msgbuf, 
            "*** Error: the pipe #%d->#%d does not exist yet. ***\n",
            from_uid,
            to_uid);

    msg_tell(sock, msgbuf);
}

void msg_err_user_not_exists(int sock, uint32_t uid)
{
    char msgbuf[0x30];

    sprintf(msgbuf, 
            "*** Error: user #%d does not exist yet. ***\n",
            uid);

    msg_tell(sock, msgbuf);
}
