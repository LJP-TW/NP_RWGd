#ifndef PROMPT_H
#define PROMPT_H

#include <stdint.h>

#include "user.h"

// Outputing prompt
extern void prompt(int sock);

extern void msg_motd(int sock);

extern void msg_broadcast(char *msg);

extern void msg_broadcast_login(user_node *node);

extern void msg_broadcast_logout(user_node *node);

extern void msg_broadcast_up_recv(char *cmd_line, user *from_user, user *to_user);

extern void msg_broadcast_up_send(char *cmd_line, user *from_user, user *to_user);

extern void msg_tell(int sock, char *msg);

extern void msg_err_up_exists(int sock, uint32_t from_uid, uint32_t to_uid);

extern void msg_err_up_not_exists(int sock, uint32_t from_uid, uint32_t to_uid);

extern void msg_err_user_not_exists(int sock, uint32_t uid);

#endif