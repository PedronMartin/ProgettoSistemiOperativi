/***********************************************
*Matricola VR471276
*Alessandro Luca Cremasco
*Matricola VR471448
*Martin Giuseppe Pedron
*Data di realizzazione: 10/05/2024 -> ../../2024
***********************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include "TrisStruct.h"

//dichiaraioni delle funzioni
void checkParameters(int, char*[]);
void enterSession();
void waitPlayers();
void play();

//dichiarazione variabili globali
int shmid, semid;
char *playername;
int player;
struct Tris *game;

int main(int argc, char *argv[]){

    //controllo i valori inseriti da terminale
    checkParameters(argc, argv);

    //salvo il nome del giocatore
    playername = argv[1];

    //comunica al server che sono entrato
    enterSession();

    //accedo alla memoria condivisa e attendo un avversario
    waitPlayers();

    //funzione di gioco
    play();

   //a ogni turno stampa matrice (mem. condivisa)

   //chiede dove inserire suo segno (controlla se rientra nella matrice o va fuori, oppure ancora è in una casella già occupata)

   //gestire il Ctrl ^C del client come che perde per abbandono (manda segnale a server)

   //gestire quando e se viene chiuso il terminale in cui è in esecuzione server o uno dei client

    //se da riga di comando dopo <nome_utente> viene inserito '*' viene fatta una fork che gioca in modo casuale

    return 0;
}

void checkParameters(int argc, char *argv[]){
    if(argc != 2 && argc != 3){
        printf("\nFactor esecuzione errato.\nFormato richiesto: ./eseguibile <nome_utente> oppure ./eseguibile <nome_utente> *\n\n");
        exit(0);
    }
    if(argc == 3 && *argv[2] != '*'){
        printf("\nFormato richiesto: ./eseguibile <nome_utente> oppure ./eseguibile <nome_utente> *\n\n");
        exit(0);
    }
}

void enterSession(){
    key_t key = ftok("TriServer.c", 111);                               //accedo ai semafori creati dal server
    if(key == -1){
        printf("\nErrore nella generazione della chiave.\n");
        exit(0);
    }
    semid = semget(key, 1, 0666);
    if(semid == -1){
        printf("\nErrore nell'accesso ai semafori.\n");
        exit(0);
    }
    struct sembuf sops = {0, 1, 0};                                     //inizializzo la struttura per l'operazione (+1)
    if(semop(semid, &sops, 1) == -1)                                    //eseguo l'operazione sul semaforo
        printf("\nErrore nell'attesa dei giocatori.\n");                //gestione errore
    printf("\nPlayer %s connesso.\n", playername);                      //comunico al player che è connesso
}

void waitPlayers(){
    game = (struct Tris*)shmat(shmid, NULL, 0);                         //attacco memoria condivisa a processo
    if(game == (void*)-1){
        printf("\nErrore nell'accesso alla memoria condivisa.\n");
        exit(0);
    }
    pthread_mutex_lock(&game->mutex);                                   //provo ad entrare in SC
    if(game->pid_p[0] == -1){                                           //se il primo giocatore non è ancora entrato
        game->pid_p[0] = getpid();                                      //significa che sono io il primo giocatore
        player = 0;                                                     //imposto il player locale
        pthread_mutex_unlock(&game->mutex);                             //esco dalla SC
        printf("\nIn attesa del secondo giocatore...\n");               //comunico al player che è in attesa
        while(game->pid_p[1] == -1);                                    //attendo il secondo giocatore
    }
    else{                                                               //se il primo giocatore è già entrato
        game->pid_p[1] = getpid();                                      //significa che sono il secondo giocatore
        player = 1;                                                     //imposto il player locale
        pthread_mutex_unlock(&game->mutex);                             //esco dalla SC
    }
    printf("\nIl gioco può iniziare. Tu sei il player con simbolo %c\n", game->simbolo[player]);
}

void play(){
    int row, column;
    while(1){                                                           //funzione di gioco         
        pause();                                                        //attendo il segnale SIGUSR1
        printf("\nInserisci riga e colonna dove inserire il simbolo: ");
        scanf("%d %d", &row, &column);                                  //inserimento riga e colonna
        if(row < 0 || row > righe - 1 || column < 0 || column > colonne - 1){             
            printf("\nValori inseriti non validi.\n");
            continue;                                                   //se i valori non sono validi ripeto il ciclo
        }
        pthread_mutex_lock(&game->mutex);                               //entro in SC
        if(game->board[row][column] != -1){                             //se la casella è già occupata
            printf("\nCasella già occupata.\n");
            pthread_mutex_unlock(&game->mutex);                         //esco da SC
            continue;                                                   //ripeto il ciclo
        }
        game->board[row][column] = player;                              //inserisco il simbolo nella matrice
        //aggiungere stampa sia qui che all'inizio del turno
        pthread_mutex_unlock(&game->mutex);                             //esco da SC
        kill(game->pid_p[server], SIGUSR1);                            //invio il segnale al server

        //utilizzare alarm e SIGALRM per gestire il timer di gioco

    }                       
}