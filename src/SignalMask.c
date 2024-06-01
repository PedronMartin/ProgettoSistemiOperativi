/***********************************************
*Matricola VR471276
*Alessandro Luca Cremasco
*Matricola VR471448
*Martin Giuseppe Pedron
*Data di realizzazione: 10/05/2024 -> 01/06/2024
***********************************************/

#include <signal.h>
#include "SignalMask.h"

sigset_t signalMask(){
    sigset_t mask;                          //set maschere
    sigfillset(&mask);                      //setta maschera con tutti i segnali
    sigdelset(&mask, SIGINT);               //rimuove SIGINT dalla maschera
    sigdelset(&mask, SIGHUP);               //rimuove SIGHUP dalla maschera
    sigdelset(&mask, SIGTERM);              //rimuove SIGTERM dalla maschera
    sigdelset(&mask, SIGALRM);              //rimuove SIGALRM dalla maschera
    sigdelset(&mask, SIGUSR1);              //rimuove SIGUSR1 dalla maschera
    sigdelset(&mask, SIGUSR2);              //rimuove SIGUSR2 dalla maschera

    return mask;
}