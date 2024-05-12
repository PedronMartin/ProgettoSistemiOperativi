/***********************************************
*Matricola VRVR471276
*Alessandro Luca Cremasco
*Matricola VR471448
*Martin Giuseppe Pedron
*Data di realizzazione: 10/05/2024 -> ../../2024
***********************************************/

//librerie necessarie
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>

//dichiaraioni delle funzioni
void checkParameters(int, char*[]);
bool memoryCreation();
void waitPlayers();
void memoryClosing();

//dichiarazione variabili globali
int shmid, semid;

int main(int argc, char *argv[]){

    //controllo il numero dei valori inseriti da terminale
    checkParameters(argc, argv);

    //creazione memoria condivisa per comunicazione tra i processi
    if(!memoryCreation()){
        memoryClosing();
        exit(0);
    }

    //attesa dei giocatori
    waitPlayers();

    //comunico ai due processi il loro simbolo di gioco

    //controllo per memoria condivisa o altri elementi già presenti (e quindi chiusi malamente in un esecuzione precedente)

    //istanzia memoria condivisa gioco (matrice di gioco deve essere qui)

    //gestione del segnale Ctrl ^C

    //gestione del segnale che avvisa (corretto e coerente) i processi che l'esecuzione termina per causa esterna (Ctrl ^C) per terminale chiuso

    //gestire segnale da client (se mette Ctrl ^C) perde per abbandono, quindi avvisa l'altro giocatore che ha vinto per abbandono

    //chiude tutta la memoria condivisa usata
    memoryClosing();

    return 0;
}   

void checkParameters(int argc, char *argv[]){
    if(argc != 4){
        printf("\nFactor esecuzione errato.\nFormato richiesto: ./eseguibile <tempo_timeout_mossa> <simbolo_p1> <_simbolo_p2>.\n\n");
        exit(0);
    }
    int timeout = atoi(argv[1]);                                      //converto il tempo di timeout in intero
    if(timeout < 0){                                                  //controllo che il tempo di timeout sia un intero positivo
        printf("\nIl tempo di timeout della mossa deve essere un intero positivo (0 se non lo si vuole).\n");
        exit(0);
    }
    char p1 = *argv[2];                                                //assegno il simbolo del player 1
    char p2 = *argv[3];                                                //assegno il simbolo del player 2
    if(p1 == p2){                                                      //controllo che i simboli dei due player siano diversi
        printf("\nI simboli dei due giocatori devono essere diversi.\n");
        exit(0);
    }
}

bool memoryCreation(){
    bool result = false;
    key_t key = ftok("TriServer.c", 111);                              //creo la chiave unica per IPC
    if(key == -1) {                                                    //gestione errore
        printf("\nErrore nella creazione della chiave.\n");
        exit(0);
    }
    shmid = shmget(key, 1024, 0666 | IPC_CREAT | IPC_EXCL);            //creo la memoria condivisa (fallisce se già esistente)
    if(shmid == -1 && errno == EEXIST){                                 //gestione errore
        printf("\nErrore nella creazione della memoria condivisa.\n");
        printf("Probabile causa: memoria condivisa già esistente.\n");
        shmid = shmget(key, 1024, 0666);                               //accedo alla memoria condivisa già esistente
        result = false;
    }
    else
        result = true;
    semid = semget(key, 1, 0666 | IPC_CREAT | IPC_EXCL);               //creo i semafori (fallisce se già esistente)       
    if(semid == -1 && errno == EEXIST){                                 //gestione errore
        printf("\nErrore nella creazione dei semafori.\n");
        printf("Probabile causa: semafori già esistenti.\n");
        semid = semget(key, 1, 0666);                                  //accedo ai semafori già esistenti
        result = false;
    }

    return result;
}

void waitPlayers(){
    printf("\nAttesa dei giocatori...\n");
    if(semctl(semid, 0, SETVAL, 0) == -1)                              //inizializzo il semaforo a 0 (come i player attuali)
        printf("\nErrore nell'inizializzazione del semaforo.\n");      //gestione errore
    struct sembuf sops = {0, -1, 0};                                   //inizializzo la struttura per la wait
    if(semop(semid, &sops, 1) == -1)                                   //eseguo la wait
        printf("\nErrore nell'attesa dei giocatori.\n");               //gestione errore
    printf("\nPlayer 1 connesso.\n");
    if(semop(semid, &sops, 1) == -1)                                   //eseguo la wait
        printf("\nErrore nell'attesa dei giocatori.\n");               //gestione errore
    printf("\nPlayer 2 connesso.\n");
}

void memoryClosing(){
    if(shmctl(shmid, IPC_RMID, NULL) == 0);                            //chiudo la memoria condivisa (null è puntatore a struttura di controllo, non necessario)
        printf("\nMemoria condivisa chiusa.\n");
    if(semctl(semid, 0, IPC_RMID) == 0)                                //chiudo i semafori
        printf("Semafori chiusi.\n");
}   