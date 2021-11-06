#include <stdio.h>
#include <fcntl.h>

#include "prng.h"

void get_rand(void *buf, int buflen)
{
    int fd = open("/dev/urandom", O_RDONLY);

    read(fd, buf, buflen);

    close(fd);
}
