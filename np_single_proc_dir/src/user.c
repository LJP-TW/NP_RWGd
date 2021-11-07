#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "user.h"
#include "prng.h"
#include "prompt.h"

user_list *all_users;

static user_node* user_node_init(user *user)
{
    user_node *node = malloc(sizeof(user_node));

    node->next = NULL;
    node->prev = NULL;
    node->user = user;

    return node;
}

static user* user_init(struct sockaddr_in caddr, int sock)
{
    user *u = malloc(sizeof(user));

    u->uid = uid_alloc(all_users->unused_uid);
    u->sock = sock;
    get_rand(&(u->sid), sizeof(u->sid));
    u->port = ntohs(caddr.sin_port);
    u->envp_list = envp_list_init();

    // Initial envp
    envp_list_insert(u->envp_list, "PATH", "bin:.");

    u->np_list = nplist_init();
    u->up_list = uplist_init();
    strcpy(u->ip, inet_ntoa(caddr.sin_addr));
    strcpy(u->name, "(no name)");

    return u;
}

static void user_release(user *user)
{
    uid_release(all_users->unused_uid, user->uid);

    if (user->envp_list) {
        envp_list_release(user->envp_list);
    }

    if (user->np_list) {
        nplist_release(user->np_list);
    }

    if (user->up_list) {
        uplist_release(user->up_list);
    }

    free(user);
}

void user_list_init(void)
{
    all_users = malloc(sizeof(user_list));

    all_users->head = NULL;
    all_users->tail = &(all_users->head);
    all_users->unused_uid = unused_uid_init();
    all_users->cnt  = 0;
}

user_node* user_list_insert(struct sockaddr_in caddr, int sock)
{
    user *user = user_init(caddr, sock);
    user_node *node = user_node_init(user);
    user_node *tail = (user_node *)(all_users->tail);

    all_users->cnt += 1;

    if (!all_users->head || tail->user->uid < user->uid) {
        // Append; update tail
        node->prev = (user_node *)all_users->tail; // next offset = 0
        *(all_users->tail) = node;
        all_users->tail = &(node->next);
        return node;
    } 

    // Find position; don't update tail
    while ((user_node **)(tail = tail->prev) != &(all_users->head)) {
        if (tail->user->uid < user->uid) {
            node->next = tail->next;
            tail->next->prev = node;
            tail->next = node;
            node->prev = tail;
            return node;            
        }
    }

    // Update head
    node->prev = (user_node *)&(all_users->head);
    node->next = all_users->head;
    all_users->head->prev = node;
    all_users->head = node;

    return node;
}

void user_list_remove(user_node *node)
{
    node->prev->next = node->next;

    if (node->next) {
        node->next->prev = node->prev;
    } else {
        all_users->tail = &(node->prev->next);
    }

    user_release(node->user);
    free(node);
}

user_node* user_list_find_by_sock(int sock)
{
    user_node *cur = all_users->head;

    while (cur) {
        if (cur->user->sock == sock) {
            return cur;
        }

        cur = cur->next;
    }

    return NULL;
}

user_node* user_list_find_by_uid(uint32_t uid)
{
    user_node *cur = all_users->head;

    while (cur) {
        if (cur->user->uid == uid) {
            return cur;
        }

        cur = cur->next;
    }

    return NULL;
}

user_node* user_list_find_by_name(char *name)
{
    user_node *cur = all_users->head;

    while (cur) {
        if (!strcmp(cur->user->name, name)) {
            return cur;
        }

        cur = cur->next;
    }

    return NULL;
}

void user_cmd_setenv(user *user, char *key, char *value)
{
    envp_node *node = envp_list_find(user->envp_list, key);

    if (node) {
        free(node->value);
        node->value = strdup(value);
    } else {
        envp_list_insert(user->envp_list, key, value);
    }
}

void user_cmd_printenv(user *user, char *key)
{
    char buf[0x100] = { 0 };
    envp_node *node = envp_list_find(user->envp_list, key);

    if (node) {
        sprintf(buf, "%s\n", node->value);
        msg_tell(user->sock, buf);
    }
}

void user_cmd_who(user *user)
{
    char buf[0x30] = { 0 };
    
    msg_tell(user->sock, "<ID>\t<nickname>\t<IP:port>\t<indicate me>\n");

    user_node *cur = all_users->head;

    while (cur) {
        // ID
        sprintf(buf, "%d\t", cur->user->uid);
        msg_tell(user->sock, buf);

        // Nickname
        sprintf(buf, "%s\t", cur->user->name);
        msg_tell(user->sock, buf);

        // IP:Port
        sprintf(buf, "%s:%d\t", cur->user->ip, cur->user->port);
        msg_tell(user->sock, buf);

        // Is me?
        if (cur->user->uid == user->uid) {
            msg_tell(user->sock, "<-me");
        }
        
        msg_tell(user->sock, "\n");

        cur = cur->next;
    }
}

void user_cmd_tell(user *user, uint32_t to_uid, char *msg)
{
    char buf[0x40] = { 0 };
    user_node *node = user_list_find_by_uid(to_uid);

    if (node) {
        sprintf(buf, "*** %s told you ***: ", user->name);

        msg_tell(node->user->sock, buf);
        msg_tell(node->user->sock, msg);
        msg_tell(node->user->sock, "\n");
    } else {
        sprintf(buf, "*** Error: user #%d does not exist yet. ***\n", to_uid);
        msg_tell(user->sock, buf);
    }
}

void user_cmd_yell(user *user, char *msg)
{
    char buf[0x40] = { 0 };

    sprintf(buf, "*** %s yelled ***:  ", user->name);

    msg_broadcast(buf);
    msg_broadcast(msg);
    msg_broadcast("\n");
}

void user_cmd_name(user *user, char *new_name)
{
    char buf[0x50] = { 0 };
    user_node *node = user_list_find_by_name(new_name);

    if (node) {
        sprintf(buf, "*** User '%s' already exists. ***\n", new_name);
        msg_tell(user->sock, buf);
    } else {
        strcpy(user->name, new_name);

        // e.g. *** User from 140.113.215.62:1201 is named 'Mike'. ***
        sprintf(buf, "*** User from %s:%d is named '", user->ip, user->port);
        msg_broadcast(buf);
        msg_broadcast(new_name);
        msg_broadcast("'. ***\n");
    }
}
