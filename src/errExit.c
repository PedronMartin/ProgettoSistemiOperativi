#include "errorExit.h"
#include <stdio.h>
#include <errno.h>

void errExit(const char *msg) {
    perror(msg);
}