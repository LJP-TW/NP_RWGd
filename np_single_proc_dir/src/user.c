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
    envp_list_insert(u->envp_list, "PATH", "/bin");

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

    node->prev = (user_node *)all_users->tail; // next offset = 0
    *(all_users->tail) = node;
    all_users->tail = &(node->next);

    all_users->cnt += 1;

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

void user_setenv(user *user, char *key, char *value)
{
    envp_node *node = envp_list_find(user->envp_list, key);

    if (node) {
        free(node->value);
        node->value = strdup(value);
    } else {
        envp_list_insert(user->envp_list, key, value);
    }
}

void user_printenv(user *user, char *key)
{
    char buf[0x100] = { 0 };
    envp_node *node = envp_list_find(user->envp_list, key);

    if (node) {
        sprintf(buf, "%s\n", node->value);
        msg_tell(user->sock, buf);
    }
}
