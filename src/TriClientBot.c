/***********************************************
*Matricola VR471276
*Alessandro Luca Cremasco
*Matricola VR471448
*Martin Giuseppe Pedron
*Data di realizzazione: 10/05/2024 -> 30/05/2024
***********************************************/

//librerie richieste
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include "TrisStruct.h"
#include "errorExit.h"
#include "SignalMask.h"

//dichiaraioni delle funzioni
void signalManage();
void enterSession();
void waitPlayers();
void play();
void victory();
void sigTimeout();
void sigIntManage(int);
void signalToServer();
void sigFromServer(int);
void closeErrorGame();
void closeGameSuccessfull();

//dichiarazione variabili globali
int shmid, semid;
int player;
int enemy;
int row, column;
struct Tris *game;

int main(){

    //inizializzazione del generatore di numeri casuali
    srand(time(NULL));

    //settaggio della maschera dei segnali
    sigset_t mask = signalMask();
    if(sigprocmask(SIG_SETMASK, &mask, NULL) == -1){
        errorExit("\nErrore nella settaggio della maschera dei segnali.\n");
        exit(EXIT_FAILURE);
    }

    //gestione dei segnali
    signalManage();

    //comunica al server che sono entrato
    enterSession();

    //accedo alla memoria condivisa e attendo un avversario
    waitPlayers();

    //funzione di gioco principale
    play();

    //uscita dal gioco
    closeGameSuccessfull();

}

void signalManage(){
    if(signal(SIGALRM, sigTimeout) == SIG_ERR){                         //gestione del segnale SIGALRM
        errorExit("\nErrore nella gestione del segnale SIGALRM.\n");
        exit(EXIT_FAILURE);
    }
                                                                        //gestione del segnale SIGINT e SIGHUP
    if(signal(SIGINT, sigIntManage) == SIG_ERR || signal(SIGHUP, sigIntManage) == SIG_ERR || signal(SIGTERM, sigIntManage) == SIG_ERR){
        errorExit("\nErrore nella gestione del segnale SIGINT, SIGHUP o SIGTERM.\n");
        exit(EXIT_FAILURE);
    }
                                                                        //gestione del segnale SIGUSR1 e SIGUSR2
    if(signal(SIGUSR1, sigFromServer) == SIG_ERR || signal(SIGUSR2, sigFromServer) == SIG_ERR){
        errorExit("\nErrore nella gestione del segnale SIGUSR1 e SIGUSR2.\n");
        exit(EXIT_FAILURE);
    }
}

void enterSession(){
    key_t key = ftok("../src/TriServer.c", 111);                        //accedo ai semafori creati dal server
    if(key == -1){
        errorExit("\nErrore nella generazione della chiave.\n");
        exit(EXIT_FAILURE);
    }
    semid = semget(key, 3, 0666);
    if(semid == -1){
        errorExit("\nErrore nell'accesso ai semafori.\n");
        exit(EXIT_FAILURE);
    }
    shmid = shmget(key, sizeof(game), 0666);                           //accedo alla memoria condivisa creata dal server
    if(shmid == -1){
        errorExit("\nErrore nell'accesso alla memoria condivisa.\n");
        exit(EXIT_FAILURE);
    }
}

void waitPlayers(){
    game = (struct Tris*)shmat(shmid, NULL, 0);                         //attacco memoria condivisa a processo
    if(game == (void*)-1){
        errorExit("\nErrore nell'accesso alla memoria condivisa.\n");
        exit(EXIT_FAILURE);
    }
    pthread_mutex_lock(&game->mutex);                                   //provo ad entrare in SC
    game->pid_p2 = getpid();                                            //di default siamo secondo giocatore
    player = 1;                                                         //imposto il player locale
    enemy = 0;                                                          //imposto il player avversario
    pthread_mutex_unlock(&game->mutex);                                 //esco dalla SC
    struct sembuf sops = {0, 2, 0};                                     //inizializzo la struttura per l'operazione (+2)
    if(semop(semid, &sops, 1) == -1){                                   //comunichiamo a Server e P1 che siamo entrati come P2
        errorExit("\nErrore nella comunicazione della connessione.\n");
        closeErrorGame();
    }
}

void play(){
    struct sembuf sops1 = {player + 1, -1, 0};                          //struttura per l'operazione (-1) in base a che processo sono
    struct sembuf sops2 = {player + 1, +1, 0};                          //struttura per l'operazione (+1) in base a che processo sono
    if(semop(semid, &sops1, 1) == -1){                                  //attendo che il server mi dia il via (no while perchè chiude in tutti i segnali che può ricevere)
        errorExit("\nErrore nell'attesa del turno.\n");                 //gestione errore
        closeErrorGame();
    }
    while(game->winner == -1){
        int flag = 1;
        do{
            flag = 1;
            row = rand() % righe;                                       //genero una riga casuale
            column = rand() % colonne;                                  //genero una colonna casuale
            if(game->board[row][column] != -1){                         //se la casella è già occupata ripeto il ciclo
                flag = 0;
                continue;
            }
        }while(!flag);

        pthread_mutex_lock(&game->mutex);                               //entro in SC
        game->board[row][column] = player;                              //inserisco il simbolo nella matrice
        pthread_mutex_unlock(&game->mutex);                             //esco da SC
        sleep(1);                                                       //attendo un secondo
        if(semop(semid, &sops2, 1) == -1){                              //comunico al server che ho finito il turno
            errorExit("\nErrore nella comunicazione di fine turno\n");  //gestione errore
            closeErrorGame();
        }
        if(semop(semid, &sops1, 1) == -1){                              //attendo che il server mi dia il via (no while)
            errorExit("\nErrore nell'attesa del turno.\n");             //gestione errore
            closeErrorGame();
        }
    }
}

void sigTimeout(){                                                      //se scade il tempo a uno dei due giocatori, la partita finisce                                                         //comunico il vincitore
    closeGameSuccessfull();
}

void sigIntManage(int sig){
    pthread_mutex_lock(&game->mutex);                                   //entro in SC
    if(player)                                                          //in base a se sono player 2 o player 1
        game->pid_p2 = -1;
    else
        game->pid_p1 = -1;                                              //comunico abbandono a Server
    pthread_mutex_unlock(&game->mutex);                                 //esco da SC
    signalToServer();
    closeGameSuccessfull();
}

void signalToServer(){
    if(kill(game->pid_s, SIGUSR1) == -1){                              //comunico al server che ho abbandonato la partita avviata
        errorExit("\nErrore nella comunicazione con il server.\n");
        closeErrorGame();
    }
}

void sigFromServer(int sig){
    closeGameSuccessfull();
}

void closeErrorGame(){
    if(shmdt(game) == -1){                                             //tentativo di stacco della memoria condivisa
        errorExit("\nErrore nello stacco della memoria condivisa.\n");
        exit(EXIT_FAILURE);
    }  
    exit(EXIT_FAILURE);
}

void closeGameSuccessfull(){
    if(shmdt(game) == -1){                                             //tentativo di stacco della memoria condivisa
        errorExit("\nErrore nello stacco della memoria condivisa.\n");
        exit(EXIT_FAILURE);
    } 
    exit(EXIT_SUCCESS);
}