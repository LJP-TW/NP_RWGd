#ifndef ENVP_H
#define ENVP_H

#include <stdint.h>

typedef struct envp_node_tag envp_node;
struct envp_node_tag {
    envp_node *next;
    char *key;
    char *value;
};

typedef struct envp_list_tag envp_list;
struct envp_list_tag {
    envp_node *head;
    envp_node **tail;
    uint32_t cnt;
};

extern envp_node* envp_node_init(char *key, char *value);

extern void envp_list_insert(envp_list *list, char *key, char *value);

extern envp_list* envp_list_init(void);

extern void envp_list_release(envp_list *list);

extern envp_node* envp_list_find(envp_list *list, char *key);

#endif