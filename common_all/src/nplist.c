#include <stdlib.h>

#include "nplist.h"

static np_node* npnode_init(int numbered)
{
    np_node *new_np = malloc(sizeof(np_node));

    new_np->next = NULL;
    new_np->plist = NULL;
    new_np->numbered = numbered;
    pipe(new_np->fd);

    return new_np;
}

static void npnode_release(np_node *node)
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

np_list* nplist_init(void)
{
    np_list *list = malloc(sizeof(np_list));

    list->head = NULL;
    list->tail = &(list->head);
    list->cnt = 0;

    return list;
}

void nplist_release(np_list *list)
{
    np_node *cur;

    while ((cur = list->head)) {
        list->head = cur->next;
        npnode_release(cur);
    }

    free(list);
}

np_node* nplist_find_by_numbered(np_list *list, int numbered)
{
    np_node **np_ptr;
    np_node *np_cur;

    // Find corresponding numbered pipe
    np_ptr = &(list->head);
    while ((np_cur = *np_ptr)) {
        np_ptr = &(np_cur->next);

        if (np_cur->numbered == numbered) {
            break;
        }
    }

    return np_cur;
}

void nplist_remove_by_numbered(np_list *list, int numbered)
{
    np_node **np_ptr;
    np_node *np_cur;
    
    // Find corresponding numbered pipe
    np_ptr = &(list->head);
    while ((np_cur = *np_ptr)) {
        if (np_cur->numbered == numbered) {
            // Keep chain
            if (!np_cur->next) {
                list->tail = np_ptr;
            }
            *np_ptr = np_cur->next;
            
            // Free it
            if (np_cur->fd[0] != -1)
                close(np_cur->fd[0]);
            if (np_cur->fd[1] != -1)
                close(np_cur->fd[1]);
            plist_release(np_cur->plist);
            free(np_cur);

            // End
            break;
        } 
        else {
            np_ptr = &(np_cur->next);
        }
    }
}

np_node* nplist_insert(np_list *list, int numbered)
{
    np_node *new_np = npnode_init(numbered);

    // Insert
    *(list->tail) = new_np;
    list->tail = &(new_np->next);
    list->cnt += 1;

    return new_np;
}

void nplist_update(np_list *list)
{
    np_node **np_ptr;
    np_node *np_cur;

    // Find corresponding numbered pipe
    np_ptr = &(list->head);
    while ((np_cur = *np_ptr)) {
        if (--np_cur->numbered == -1) {
            // Keep chain
            *np_ptr = np_cur->next;

            // Free it
            if (np_cur->fd[0] != -1)
                close(np_cur->fd[0]);
            if (np_cur->fd[1] != -1)
                close(np_cur->fd[1]);
            plist_release(np_cur->plist);
            free(np_cur);
        } 
        else {
            np_ptr = &(np_cur->next);
        }
    }

    list->tail = np_ptr;
}