#include <stdlib.h>
#include <string.h>

#include "nplist.h"
#include "unused_uid.h"
#include "user.h"
#include "prng.h"

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