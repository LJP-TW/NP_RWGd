#ifndef NPLIST_H
#define NPLIST_H

#include "pidlist.h"

typedef struct numbered_pipe_node_tag np_node;
struct numbered_pipe_node_tag {
    np_node *next;
    pid_list *plist;
    int numbered;
    int fd[2];
};

typedef struct numbered_pipe_list_tag np_list;
struct numbered_pipe_list_tag {
    np_node *head;
    np_node **tail;
    int cnt;
};

extern np_list* nplist_init(void);

extern void nplist_release(np_list *list);

extern np_node* nplist_find_by_numbered(np_list *list, int numbered);

extern void nplist_remove_by_numbered(np_list *list, int numbered);

extern np_node* nplist_insert(np_list *list, int numbered);

extern void nplist_update(np_list *list);

#endif