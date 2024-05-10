/***********************************************
*Matricola VR
*Alessandro Luca Cremasco
*Matricola VR471448
*Martin Giuseppe Pedron
*Data di realizzazione: 10/05/2024 -> ../../2024
***********************************************/

//librerie necessarie
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>

//dichiaraioni delle funzioni
void checkParameters(int);
void memoryCreation();

void memoryClosing();

int main(int argc, char *argv[]){

    //controllo il numero dei valori inseriti da terminale
    checkParameters(argc);

    //creazione memoria condivisa per comunicazione tra i processi
    memoryCreation();

    //attesa dei giocatori
    printf("Attesa dei giocatori...\n");
    struct sembuf sem_op = {0, -1, 0};                                      //inizializzo la struttura per l'operazione da fare
    semop(semid, &sem_op, 1);                                               //attendo che il semaforo sia uguale a 0

    //comunico ai due processi il loro simbolo di gioco

    //controllo per memoria condivisa o altri elementi già presenti (e quindi chiusi malamente in un esecuzione precedente)

    //istanzia memoria condivisa gioco (matrice di gioco deve essere qui)

    //gestione del segnale Ctrl ^C

    //gestione del segnale che avvisa (corretto e coerente) i processi che l'esecuzione termina per causa esterna (Ctrl ^C)

    //gestire segnale da client (se mette Ctrl ^C) perde per abbandono, quindi avvisa l'altro giocatore che ha vinto per abbandono

    //se da riga di comando dopo <nome_utente> viene inserito '*' viene fatta una fork che gioca in modo casuale

    //chiude tutta la memoria condivisa usata
    memoryClosing();

    return 0;
}   

void checkParameters(int argc){
    if(argc != 4){
        printf("\nFactor esecuzione errato.\nFormato richiesto: ./eseguibile <tempo_timeout_mossa> <simbolo_p1> <_simbolo_p2>\n\n");
        exit(0);
    }
}

void memoryCreation(){
    key_t key = ftok("TriServer.c", 65);                               //creo la chiave per la memoria condivisa
    int shmid = shmget(key, 1024, 0666|IPC_CREAT);                     //creo la memoria condivisa
    if(shmid == -1){                                                   //gestione errore
        printf("\nErrore nella creazione della memoria condivisa.\n");
        exit(0);
    }
    //creo i semafori      ...                                                
}

void memoryClosing(){
    shmctl(shmid, IPC_RMID, NULL);                                     //chiudo la memoria condivisa
    semctl(semid, 0, IPC_RMID);                                        //chiudo i semafori
}