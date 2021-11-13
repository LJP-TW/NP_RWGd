#include <stdlib.h>
#include <sys/shm.h>

#include "unused_uid.h"

static uint32_t rmsb_to_uid(uint32_t rmsb)
{
    uint32_t filter = 0xffff;
    uint32_t uid = 0;
    int f = 16;

    while (f) {
        if (!(rmsb & filter)) {
            rmsb >>= f;
            uid += f;
        }
        f >>= 1;
        filter >>= f;
    }

    return uid;
}

void unused_uid_init(unused_uid *uu)
{
    uu->shmid = shmget(IPC_PRIVATE, sizeof(uint32_t), IPC_CREAT | IPC_EXCL | 0600);
    
    uu->uidmap = shmat(uu->shmid, NULL, 0);

    *(uu->uidmap) = 0xffffffff;
}

void unused_uid_release(unused_uid *uu)
{
    shmctl(uu->shmid, IPC_RMID, NULL);
}

uint32_t uid_alloc(unused_uid *uu)
{
    // Make sure you have acquired semaphore of user_manager before 
    // enter this function.

    // Find right most set bit in bitmap
    uint32_t bitmap = *(uu->uidmap);
    uint32_t rmsb = bitmap ^ (bitmap & (bitmap - 1));

    // printf("[>] uid_alloc: %x\n", bitmap);

    *(uu->uidmap) ^= rmsb;

    return rmsb_to_uid(rmsb) + 1;
}

void uid_release(unused_uid *uu, uint32_t uid)
{
    // Make sure you have acquired semaphore of user_manager before 
    // enter this function.

    *(uu->uidmap) |= (1 << (uid - 1));
}