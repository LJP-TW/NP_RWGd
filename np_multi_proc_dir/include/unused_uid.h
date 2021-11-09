#ifndef UNUSED_UID_H
#define UNUSED_UID_H

#include <stdint.h>

// Sort from large uid to small uid
typedef struct unused_uid_tag unused_uid;
struct unused_uid_tag {
    // Maximum number of connections is 32
    // When n-th bit is 0, it means that uid n is being used.
    // uidmap resides in shared memory
    uint32_t *uidmap;

    int shmid; // shared memory id
};

extern void unused_uid_init(unused_uid *uu);

extern void unused_uid_release(unused_uid *uu);

extern uint32_t uid_alloc(unused_uid *uu);

extern void uid_release(unused_uid *uu, uint32_t uid);

#endif