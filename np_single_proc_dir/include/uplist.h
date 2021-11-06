#ifndef UPLIST_H
#define UPLIST_H

#include <stdint.h>

#include "pidlist.h"

typedef struct user_tag user;

typedef struct user_pipe_node_tag up_node;
struct user_pipe_node_tag {
    up_node *next;
    up_node *prev;

    // The order of above member cannot be changed

    pid_list *plist;
    uint32_t uid; // pipe to uid
    uint32_t sid; // pipe to session id
    int fd[2];
};

typedef struct user_pipe_list_tag up_list;
struct user_pipe_list_tag {
    up_node *head;
    up_node **tail;
    int cnt;
};

extern up_list* uplist_init(void);

extern void uplist_release(up_list *list);

extern up_node* uplist_find(up_list *list, user *user);

extern up_node* uplist_insert(up_list *list, user *to_user);

extern void uplist_remove(up_list *list, up_node *node);

#endif