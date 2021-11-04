#ifndef NETIO_H
#define NETIO_H

#include <unistd.h>

extern int net_write(int sock, char *buf, size_t len);
extern int net_read(int sock, char *buf, size_t len);

#endif