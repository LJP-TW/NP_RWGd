#include <stdio.h>

#include "netio.h"
#include "prompt.h"

void prompt(int sock)
{
    net_write(sock, "% ", 3);
}
