#include <stdio.h>

#include "netio.h"

int net_write(int sock, char *buf, size_t len)
{
    int retlen = write(sock, buf, len);

    return retlen;
}

int net_read(int sock, char *buf, size_t len)
{
    int retlen = read(sock, buf, len);

    return retlen;
}
