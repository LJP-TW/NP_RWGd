#include <stdlib.h>

#include "uplist.h"

static up_node* upnode_init(uint32_t uid)
{
    up_node *new_up = malloc(sizeof(up_node));

    new_up->next = NULL;
    new_up->prev = NULL;
    new_up->plist = NULL;
    new_up->uid = uid;
    pipe(new_up->fd);

    return new_up;
}

static void upnode_release(up_node *node)
{
    if (node->plist) {
        plist_release(node->plist);
    }

    if (node->fd[0]) {
        close(node->fd[0]);
    }

    if (node->fd[1]) {
        close(node->fd[1]);
    }

    free(node);
}

up_list* uplist_init(void)
{
    up_list *list = malloc(sizeof(up_list));

    list->head = NULL;
    list->tail = &(list->head);
    list->cnt = 0;

    return list;
}

void uplist_release(up_list *list)
{
    up_node *cur;

    while ((cur = list->head)) {
        list->head = cur->next;
        upnode_release(cur);
    }

    free(list);
}

up_node* uplist_find_by_uid(up_list *list, uint32_t uid)
{
    up_node *cur = list->head;

    while (cur) {
        if (cur->uid == uid) {
            return cur;
        }

        cur = cur->next;
    }

    return NULL;
}

up_node* uplist_insert(up_list *list, uint32_t uid)
{
    up_node *node = upnode_init(uid);

    node->prev = (up_node *)list->tail; // next offset = 0
    *(list->tail) = node;
    list->tail = &(node->next);

    return node;
}

void uplist_remove(up_list *list, up_node *node)
{
    node->prev->next = node->next;

    if (node->next) {
        node->next->prev = node->prev;
    } else {
        list->tail = &(node->prev->next);
    }

    upnode_release(node);
}