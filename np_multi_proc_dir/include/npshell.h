#ifndef NPSHELL_H
#define NPSHELL_H

#include <stdint.h>

extern uint32_t global_uid; // current user uid
extern int global_sock;     // current user socket

// cs: client socket
extern int npshell_run(uint32_t uid, int cs);

#endif