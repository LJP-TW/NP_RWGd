#include <unistd.h>

#include "prompt.h"
#include "netio.h"

void prompt(int sock)
{
    net_write(sock, "% ", 2);
}
