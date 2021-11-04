#ifndef UNUSED_UID_H
#define UNUSED_UID_H

#include <stdint.h>

// Sort from large uid to small uid
typedef struct unused_uid_tag unused_uid;
struct unused_uid_tag {
    // Maximum number of connections is 32
    // When n-th bit is 0, it means that uid n is being used.  
    uint32_t uidmap;
};

extern unused_uid* unused_uid_init(void);

extern uint32_t uid_alloc(unused_uid *uu);

extern void uid_release(unused_uid *uu, uint32_t uid);

#endif