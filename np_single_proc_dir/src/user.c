#include <stdlib.h>
#include <string.h>

#include "nplist.h"
#include "user.h"

user_list *all_users;

static envp_node* envp_node_init(char *key, char *value)
{
    envp_node *node = malloc(sizeof(envp_node));

    node->next  = NULL;
    node->key   = strdup(key);
    node->value = strdup(value);

    return node;
}

static void envp_list_insert(envp_list *list, char *key, char *value)
{
    envp_node *node = envp_node_init(key, value);

    *(list->tail) = node;
    list->tail = &(node->next);

    list->cnt += 1;
}

static envp_list* envp_list_init(void)
{
    envp_list *list = malloc(sizeof(envp_list));

    list->head = NULL;
    list->tail = &(list->head);
    list->cnt = 0;

    return list;
}

static void envp_list_release(envp_list *list)
{
    envp_node *cur;

    while ((cur = list->head)) {
        list->head = cur->next;
        free(cur->key);
        free(cur->value);
        free(cur);
    }

    free(list);
}

static unused_uid_list* unused_uid_list_init(void)
{
    unused_uid_list *list = malloc(sizeof(unused_uid_list));

    list->head = NULL;
    list->cnt = 0;

    return list;
}

static user_node* user_node_init(user *user)
{
    user_node *node = malloc(sizeof(user_node));

    node->next = NULL;
    node->user = user;

    return node;
}

void user_list_init(void)
{
    all_users = malloc(sizeof(user_list));

    all_users->head = NULL;
    all_users->tail = &(all_users->head);
    all_users->unused_uid_list = unused_uid_list_init();
    all_users->cnt  = 0;
}

void user_list_insert(user *user)
{
    user_node *node = user_node_init(user);

    *(all_users->tail) = node;
    all_users->tail = &(node->next);

    all_users->cnt += 1;
}

uint32_t user_list_alloc_uid(void)
{
    if (!all_users->unused_uid_list->cnt) {
        // New uid
        return all_users->cnt + 1;
    }

    // TODO: return uid from unused_uid_list
    return -1;
}

user* user_list_find_by_sock(int sock)
{
    user_node *cur = all_users->head;

    while (cur) {
        if (cur->user->sock == sock) {
            return cur->user;
        }

        cur = cur->next;
    }

    return NULL;
}

user* user_init(struct sockaddr_in caddr, int sock)
{
    user *u = malloc(sizeof(user));

    u->uid = user_list_alloc_uid();
    u->sock = sock;
    u->port = ntohs(caddr.sin_port);
    u->envp_list = envp_list_init();

    // Initial envp
    envp_list_insert(u->envp_list, "PATH", "/bin");

    u->np_list = nplist_init();
    strcpy(u->ip, inet_ntoa(caddr.sin_addr));
    strcpy(u->name, "(no name)");

    return u;
}

extern void user_release(user *user)
{
    if (user->envp_list) {
        envp_list_release(user->envp_list);
    }

    if (user->np_list) {
        nplist_release(user->np_list);
    }

    free(user);
}
