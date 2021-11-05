#include <stdio.h>
#include <string.h>

#include "prompt.h"
#include "netio.h"
#include "user.h"

void prompt(int sock)
{
    net_write(sock, "% ", 3);
}

void msg_motd(int sock)
{
    char msg[] = "***************************************\n"
                 "** Welcome to the information server **\n"
                 "***************************************\n";

    net_write(sock, msg, strlen(msg));
}

void msg_broadcast(char *msg)
{
    user_node *cur = all_users->head;

    while (cur) {
        if (cur->user->sock != -1) {
            net_write(cur->user->sock, msg, strlen(msg));
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

void msg_tell(int sock, char *msg)
{
    net_write(sock, msg, strlen(msg));
}
