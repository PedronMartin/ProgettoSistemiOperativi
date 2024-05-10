/***********************************************
*Matricola VR471276
*Alessandro Luca Cremasco
*Matricola VR471448
*Martin Giuseppe Pedron
*Data di realizzazione: 10/05/2024 -> ../../2024
***********************************************/

#include <stdio.h>
#include <stdlib.h>

//dichiaraioni delle funzioni
void checkParameters(int, char*[]);

int main(int argc, char *argv[]){

    //controllo i valori inseriti da terminale
    checkParameters(argc, argv);

   //manda segnale a server che sono entrato

   //controlla che mem. condivisa sia stata creata correttamente

   //stampa attesa secondo giocatore (segnale ricevuto da server forse?)

   //a ogni turno stampa matrice (mem. condivisa)

   //chiede dove inserire suo segno (controlla se rientra nella matrice o va fuori, oppure ancora è in una casella già occupata)

   //gestire il Ctrl ^C del client come che perde per abbandono (manda segnale a server)

   //gestire quando e se viene chiuso il terminale in cui è in esecuzione server o uno dei client

    //se da riga di comando dopo <nome_utente> viene inserito '*' viene fatta una fork che gioca in modo casuale

    /*struct sembuf sem_op = {0, -1, 0};                                 //inizializzo la struttura per l'operazione da fare
    if(semop(semid, &sem_op, 1) == -1)                                 //attendo che il semaforo sia uguale a 0
        printf("\nErrore nell'attesa dei giocatori.\n");               //gestione errore*/
    return 0;
}

void checkParameters(int argc, char *argv[]){
    if(argc != 2 || argc != 3){
        printf("\nFactor esecuzione errato.\nFormato richiesto: ./eseguibile <nome_utente> oppure ./eseguibile <nome_utente> *\n\n");
        exit(0);
    }
    if(argc == 3 && *argv[2] != '*'){
        printf("\nFormato richiesto: ./eseguibile <nome_utente> oppure ./eseguibile <nome_utente> *\n\n");
        exit(0);
    }
}