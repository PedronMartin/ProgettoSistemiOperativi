#include "errorExit.h"
#include <stdio.h>
#include <errno.h>

void errorExit(const char *msg) {
    perror(msg);
}