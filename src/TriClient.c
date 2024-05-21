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
void signalToServer();
void sigIntManage(int);
void sigFromServer(int);
void victory();
void closeGame();

//dichiarazione variabili globali
int shmid, semid;
char *playername;
int player;
int enemy;
int row, column;
struct Tris *game;

int main(int argc, char *argv[]){

    //controllo i valori inseriti da terminale
    checkParameters(argc, argv);

    //salvo il nome del giocatore
    playername = argv[1];

    //gestione del segnale SIGINT
    if(signal(SIGINT, sigIntManage) == SIG_ERR){
        printf("\nErrore nella gestione del segnale SIGINT.\n");
        exit(0);
    }

    //gestione del segnale SIGUSR1 e SIGUSR2
    if(signal(SIGUSR1, sigFromServer) == SIG_ERR || signal(SIGUSR2, sigFromServer) == SIG_ERR){
        printf("\nErrore nella gestione del segnale SIGUSR1.\n");
        exit(0);
    }

    //comunica al server che sono entrato
    enterSession();

    //accedo alla memoria condivisa e attendo un avversario
    waitPlayers();

    //funzione di gioco principale
    play();

   //gestire il Ctrl ^C del client come che perde per abbandono (manda segnale a server)

   //gestire quando e se viene chiuso il terminale in cui è in esecuzione server o uno dei client

    //se da riga di comando dopo <nome_utente> viene inserito '*' viene fatta una fork che gioca in modo casuale

    closeGame();
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
    semid = semget(key, 2, 0666);
    if(semid == -1){
        printf("\nErrore nell'accesso ai semafori.\n");
        exit(0);
    }
    shmid = shmget(key, sizeof(struct Tris), 0666);                     //accedo alla memoria condivisa creata dal server
    if(shmid == -1){
        printf("\nErrore nell'accesso alla memoria condivisa.\n");
        exit(0);
    }
    struct sembuf sops = {0, 1, 0};                                     //inizializzo la struttura per l'operazione (+1)
    if(semop(semid, &sops, 1) == -1){                                   //eseguo l'operazione sul semaforo
        printf("\nErrore nell'attesa dei giocatori.\n");                //gestione errore
        closeGame();
    }
    printf("\nPlayer %s connesso.\n", playername);                      //comunico al player che è connesso
}

void waitPlayers(){
    game = (struct Tris*)shmat(shmid, NULL, 0);                         //attacco memoria condivisa a processo
    if(game == (void*)-1){
        perror("\nErrore nell'accesso alla memoria condivisa.\n");
        closeGame();
    }
    pthread_mutex_lock(&game->mutex);                                   //provo ad entrare in SC
    if(game->pid_p1 == -1){                                             //se il primo giocatore non è ancora entrato
        game->pid_p1 = getpid();                                        //significa che sono io il primo giocatore
        player = 0;                                                     //imposto il player locale
        enemy = 1;                                                      //imposto il player avversario
        pthread_mutex_unlock(&game->mutex);                             //esco dalla SC
        printf("\nIn attesa del secondo giocatore...\n");               //comunico al player che è in attesa
        while(game->pid_p2 == -1);                                      //attendo il secondo giocatore
    }
    else{                                                               //se il primo giocatore è già entrato
        game->pid_p2 = getpid();                                        //significa che sono il secondo giocatore
        player = 1;                                                     //imposto il player locale
        enemy = 0;                                                      //imposto il player avversario
        pthread_mutex_unlock(&game->mutex);                             //esco dalla SC
    }
    printf("\nIl gioco può iniziare. Tu sei il player con simbolo %c\n", game->simbolo[player]);
    shmdt(game);                                                        //dettaccamento memoria condivisa
}

void play(){
    struct sembuf sops = {1, 1, 0};                                     //inizializzo la struttura per l'operazione (+1)
    if(semop(semid, &sops, 1) == -1){                                   //eseguo l'operazione sul semaforo
        printf("\nErrore nell'avvisare il server che siamo pronti.\n");                //gestione errore
        closeGame();
    }
    game = (struct Tris*)shmat(shmid, NULL, 0);                         //attacco memoria condivisa a processo
    if(player)
        printf("\nTurno del player avversario\n");                      //comunico il turno del player
    else
        printf("\nOra è il tuo turno\n\n");                             //comunico il turno del player
    while(game->winner == -1);                                          //funzione di gioco
    victory();                                                          //comunico il vincitore
    return;
    //utilizzare alarm e SIGALRM per gestire il timer di gioco  
}

void sigFromServer(int sig){
    if(game->currentPlayer == player){                              //se è il mio turno
        printBoard(game);                                           //stampa la matrice di gioco
        int flag = 1;
        do{
            flag = 1;
            printf("\nInserisci riga e colonna dove inserire il simbolo: ");
            scanf("%d %d", &row, &column);                         //inserimento riga e colonna
            if(row < 0 || row > righe - 1 || column < 0 || column > colonne - 1){             
                printf("\nValori inseriti non validi.\n");
                flag = 0;                                          //se i valori non sono validi ripeto il ciclo
            }
            else if(game->board[row][column] != -1){               //se la casella è già occupata
                printf("\nCasella già occupata.\n");
                flag = 0;                                          //ripeto il ciclo
            }
        }while(!flag);
        pthread_mutex_lock(&game->mutex);                           //entro in SC
        game->board[row][column] = player;                          //inserisco il simbolo nella matrice
        pthread_mutex_unlock(&game->mutex);                         //esco da SC
        printBoard(game);                                           //stampa la matrice di gioco post mossa
        signalToServer();                                           //comunico al server che ho giocato
    }
    printf("\n\nTurno del player avversario\n");                    //comunico il turno del player
    return;
}

void signalToServer(){
    if(player == 0)
        kill(game->pid_s, SIGUSR1);
    else
        kill(game->pid_s, SIGUSR2);
}

void sigIntManage(int sig){         //MANCA DOPPIA PREMITURA
    printf("\nChiusura del server in corso per causa esterna\n");
    pid_t server = game->pid_s;
    shmdt(game);
    if(kill(server, SIGUSR1) == -1)                                     //avviso il server di controllare la partita
        printf("\nErrore nella comunicazione con il server.\n");        //non importa il tipo di segnale (SIGUSR1 o SIGUSR2)
   exit(0);
}

void victory(){
    if(game->winner == player){
        if(player){
            if(game->pid_p1 == -1)
                printf("\nIl player avversario ha abbandonato la partita. Hai vinto!\n");
            else
                printf("\nHai vinto la partita!\n");
        }
        else{
            if(game->pid_p2 == -1)
                printf("\nIl player avversario ha abbandonato la partita. Hai vinto!\n");
            else
                printf("\nHai vinto la partita!\n");
        }
    }
    else
        printf("\nHai perso!\n");
    return;
}

void closeGame(){
    shmdt(game);
    exit(0);
}