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

int net_readline(int sock, char *buf, size_t max_len)
{
    int first_char = 1;
    int idx = 0;

    while (idx < max_len) {
        if (!net_read(sock, buf + idx, 1)) {
            return -1;
        }

        // Trim prefix space
        if (first_char) {
            if (buf[idx] == ' ') {
                continue;
            }
            first_char = 0;
        }

        if (buf[idx] == '\n') {
            if (buf[idx-1] == '\r') {
                buf[idx-1] = 0;
                return idx - 1;
            }
            buf[idx] = 0;
            return idx;
        }

        if (buf[idx] == 0) {
            return idx;
        }

        idx += 1;
    }

    return idx;
}