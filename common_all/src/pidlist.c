#include <stdlib.h>
#include <signal.h>

#include "pidlist.h"
#include "sys_variable.h"

pid_list *closed_plist;
pid_list *sh_closed_plist;
pid_list *alive_plist;

pid_list* plist_init()
{
    pid_list *plist = malloc(sizeof(pid_list));
    
    plist->head = NULL;
    plist->tail = &(plist->head);
    plist->len = 0;

    return plist;
}

void plist_release(pid_list *plist)
{
    pid_node **ptr = &(plist->head);
    pid_node *tmp;

    while ((tmp = *ptr)) {
        *ptr = tmp->next;

        plist_insert(alive_plist, tmp->pid);
        
        free(tmp);
    }

    free(plist);
}

void plist_insert(pid_list *plist, pid_t pid)
{
    pid_node *new_node = malloc(sizeof(pid_node));

    new_node->next = NULL;
    new_node->pid  = pid;

    *(plist->tail) = new_node;
    plist->tail = &(new_node->next);

    plist->len += 1;
}

void plist_insert_block(pid_list *plist, pid_t pid)
{
    sigset_t oldset;
    sigprocmask(SIG_BLOCK, &sigset_SIGCHLD, &oldset);

    plist_insert(plist, pid);

    sigprocmask(SIG_SETMASK, &oldset, NULL);
}

void plist_merge(pid_list *plist1, pid_list *plist2)
{
    if (!plist2->len)
        return;

    plist_delete_intersect(plist1, plist2);
    
    *(plist1->tail) = plist2->head;
    plist1->tail = plist2->tail;
    plist1->len += plist2->len;

    plist2->head = NULL;
    plist2->tail = &(plist2->head);
    plist2->len  = 0;
}

void plist_delete_intersect(pid_list *plist1, pid_list *plist2)
{
    // TODO: Optimization
    pid_node **pa;
    pid_node *ta;
    pid_node **pb;
    pid_node *tb;

    pa = &(plist1->head);

    while(plist1->len && plist2->len && (ta = *pa)) {
        int found = 0;

        pb = &(plist2->head);

        while ((tb = *pb)) {
            if (ta->pid == tb->pid) {
                *pa = ta->next;
                *pb = tb->next;
                free(ta);
                free(tb);
                plist1->len -= 1;
                plist2->len -= 1;
                found = 1;
                break;
            }

            pb = &(tb->next);
        }

        if (!found) {
            pa = &(ta->next);
        } else {
            if (!(*pa)) {
                plist1->tail = pa;
            }
            if (!(*pb)) {
                plist2->tail = pb;
            }
        }
    }
}

void plist_delete_intersect_block(pid_list *plist1, pid_list *plist2)
{
    sigset_t oldset;
    sigprocmask(SIG_BLOCK, &sigset_SIGCHLD, &oldset);

    plist_delete_intersect(plist1, plist2);

    sigprocmask(SIG_SETMASK, &oldset, NULL);
}

int plist_delete_by_pid(pid_list *plist1, pid_t pid)
{
    pid_node **pa;
    pid_node *ta;

    if (!plist1 || !plist1->len)
        return 0;

    pa = &(plist1->head);

    while((ta = *pa)) {
        if (ta->pid == pid) {
            *pa = ta->next;
            free(ta);
            plist1->len -= 1;
            if (!(*pa)) {
                plist1->tail = pa;
            }
            return 1;
        }
        pa = &(ta->next);
    }

    return 0;
}
