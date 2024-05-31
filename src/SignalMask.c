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