#ifndef PROMPT_H
#define PROMPT_H

#include "user.h"

// Outputing prompt
extern void prompt(int sock);

extern void msg_motd(int sock);

extern void msg_broadcast(char *msg);

extern void msg_broadcast_login(user_node *node);

extern void msg_broadcast_logout(user_node *node);

extern void msg_tell(int sock, char *msg);

#endif