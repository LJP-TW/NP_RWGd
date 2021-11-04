#ifndef USER_H
#define USER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "nplist.h"
#include "unused_uid.h"

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

typedef struct user_tag user;
struct user_tag {
    uint32_t uid;
    int      sock;   // socket
    uint16_t port;
    envp_list *envp_list;  // environment parameters
    np_list *np_list;      // numbered pipes
    char ip[0x10];   // xxx.xxx.xxx.xxx
    char name[0x20]; // maximum length is 20, 0x20 is for memory alignment
};

typedef struct user_node_tag user_node;
struct user_node_tag {
    user_node *next;
    user *user;
};

typedef struct user_list_tag user_list;
struct user_list_tag {
    user_node *head;
    user_node **tail;
    unused_uid *unused_uid;
    uint32_t cnt; // how many users
};

extern user_list *all_users;

extern void user_list_init(void);

extern void user_list_insert(user *user);

extern user* user_list_find_by_sock(int sock);

extern user* user_init(struct sockaddr_in caddr, int sock);

extern void user_release(user *user);

#endif