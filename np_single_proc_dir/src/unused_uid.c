#include <stdlib.h>

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

unused_uid* unused_uid_init(void)
{
    unused_uid *uu = malloc(sizeof(unused_uid));

    uu->uidmap = 0xffffffff;

    return uu;
}

uint32_t uid_alloc(unused_uid *uu)
{
    // Find right most set bit in bitmap
    uint32_t bitmap = uu->uidmap;
    uint32_t rmsb = bitmap ^ (bitmap & (bitmap - 1));

    uu->uidmap ^= rmsb;

    return rmsb_to_uid(rmsb);
}

void uid_release(unused_uid *uu, uint32_t uid)
{
    uu->uidmap |= (1 << uid);
}