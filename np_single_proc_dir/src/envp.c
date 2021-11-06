#include <stdlib.h>
#include <string.h>

#include "envp.h"

envp_node* envp_node_init(char *key, char *value)
{
    envp_node *node = malloc(sizeof(envp_node));

    node->next  = NULL;
    node->key   = strdup(key);
    node->value = strdup(value);

    return node;
}

void envp_list_insert(envp_list *list, char *key, char *value)
{
    envp_node *node = envp_node_init(key, value);

    *(list->tail) = node;
    list->tail = &(node->next);

    list->cnt += 1;
}

envp_list* envp_list_init(void)
{
    envp_list *list = malloc(sizeof(envp_list));

    list->head = NULL;
    list->tail = &(list->head);
    list->cnt = 0;

    return list;
}

void envp_list_release(envp_list *list)
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

envp_node* envp_list_find(envp_list *list, char *key)
{
    envp_node *cur = list->head;

    while (cur) {
        if (!strcmp(cur->key, key)) {
            return cur;
        }

        cur = cur->next;
    }

    return NULL;
}
