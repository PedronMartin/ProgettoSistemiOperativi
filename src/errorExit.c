/***********************************************
*Matricola VR471276
*Alessandro Luca Cremasco
*Matricola VR471448
*Martin Giuseppe Pedron
*Data di realizzazione: 10/05/2024 -> 01/06/2024
***********************************************/

#include "errorExit.h"
#include <stdio.h>
#include <errno.h>

void errorExit(const char *msg) {
    perror(msg);
}