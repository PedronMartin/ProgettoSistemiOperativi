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
void checkReferee();
void signalToServer();
void sigIntManage(int);
void sigTimeout();
void sigFromServer(int);
void victory();
void closeGame();

//dichiarazione variabili globali
int shmid, semid;
char *playername;
int player;
int enemy;
int row, column;
int flagTimeout = 0;
struct Tris *game;

int main(int argc, char *argv[]){

    //controllo i valori inseriti da terminale
    checkParameters(argc, argv);

    //salvo il nome del giocatore
    playername = argv[1];

    //gestione del segnale SIGALRM
    if(signal(SIGALRM, sigTimeout) == SIG_ERR){
        printf("\nErrore nella gestione del segnale SIGALRM.\n");
        exit(0);
    }

    //gestione del segnale SIGINT
    if(signal(SIGINT, sigIntManage) == SIG_ERR || signal(SIGHUP, sigIntManage) == SIG_ERR){
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

   //gestire quando e se viene chiuso il terminale in cui è in esecuzione server o uno dei client

    //se da riga di comando dopo <nome_utente> viene inserito '*' viene fatta una fork che gioca in modo casuale

    //uscita dal gioco
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
    semid = semget(key, 3, 0666);
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
        printf("\nIn attesa del secondo giocatore...\n");               //comunico al player che è in attesa
        pthread_mutex_unlock(&game->mutex);                                 //esco dalla SC
        while(game->pid_p2 == -1);                                      //attendo il secondo giocatore
    }
    else{                                                               //se il primo giocatore è già entrato
        game->pid_p2 = getpid();                                        //significa che sono il secondo giocatore
        player = 1;                                                     //imposto il player locale
        enemy = 0;                                                      //imposto il player avversario
        pthread_mutex_unlock(&game->mutex);                                 //esco dalla SC
    }
    printf("\nIl gioco può iniziare. Tu sei il player con simbolo %c\n", game->simbolo[player]);
    printf("Il tuo avversario è il player con simbolo %c\n", game->simbolo[enemy]);
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
    while(game->winner == -1){                                          //funzione di gioco
        pause();                                                        //attendo segnali dal server
        if(game->currentPlayer == player){                                  //se è il mio turno
            printBoard(game);                                               //stampa la matrice di gioco
            int flag = 1;
            printf("\nOra è il tuo turno. Hai %d secondi per giocare.\n", game->timeout);
            pthread_mutex_unlock(&game->mutex);
            do{
                flag = 1;
                printf("\nInserisci riga e colonna dove inserire il simbolo: ");
                scanf("%d %d", &row, &column);                              //inserimento riga e colonna
                if(flagTimeout)                                            //se il tempo è scaduto esco dal ciclo
                    break;
                if(row < 0 || row > righe - 1 || column < 0 || column > colonne - 1){             
                    printf("\nValori inseriti non validi.\n");
                    flag = 0;                                               //se i valori non sono validi ripeto il ciclo
                }
                else if(game->board[row][column] != -1){                    //se la casella è già occupata
                    printf("\nCasella già occupata.\n");
                    flag = 0;                                               //ripeto il ciclo
                }
            }while(!flag);
            
            pthread_mutex_lock(&game->mutex);                               //entro in SC
            game->board[row][column] = player;                              //inserisco il simbolo nella matrice
        }
        printBoard(game);                                               //stampa la matrice di gioco post mossa
        pthread_mutex_unlock(&game->mutex);                             //esco da SC
        signalToServer();                                               //comunico al server che ho giocato
        printf("\n\nTurno del player avversario\n");                        //comunico il turno del player
    }
    victory();                                                          //comunico il vincitore
    return;
}

void sigFromServer(int sig){
    printf("\nSEGNALE DA SERVER RICEVUTO!\n");
    checkReferee();                                                 //controllo se il server ha chiuso la partita
    victory();
    return;
}

void sigTimeout(){
    pthread_mutex_lock(&game->mutex);
    if(game->winner != player)
        printf("\nTempo scaduto. Ricorda che hai un time di %d per giocare la tua mossa.\n", game->timeout);
    victory();
}

void checkReferee(){
    if(game->pid_s == -1){                                          //se il server ha chiuso la partita
        printf("\nLa partita termina in modo anomalo perchè il server è caduto.\n");
        pthread_mutex_lock(&game->mutex);
        closeGame();
    }
    return;
}

void signalToServer(){
    struct sembuf sops = {semid, -1, 0};                // inizializzo la struttura per la wait
    if(semop(semid, &sops, 1) == -1)                    //aspettiamo che il Server sia pronto a ricevere il segnale
        printf("\nErrore nell'attesa che il Server sia pronto a ricevere.\n");
    if(player == 0)
        if(kill(game->pid_s, SIGUSR1) == -1)
            printf("\nErrore nella comunicazione con il server.\n");
    else
        if(kill(game->pid_s, SIGUSR2) == -1)
            printf("\nErrore nella comunicazione con il server.\n");
}

void sigIntManage(int sig){
    printf("\nAbbandono partita in corso...\n");
    pthread_mutex_lock(&game->mutex);                                        //entro in SC
    game->pid_p1 = -1;                                                       //comunico abbandono
    pthread_mutex_unlock(&game->mutex);                                      //esco da SC
    signalToServer();                                                        //comunico al server
    //non importa il tipo di segnale (SIGUSR1 o SIGUSR2)
    pthread_mutex_lock(&game->mutex);                                        //entro in SC
    closeGame();
}

void victory(){
    pthread_mutex_lock(&game->mutex);
    if(game->winner == player){                                              //se hai vinto
        if(player){                                                          //se sei player 2
            if(game->pid_p1 == -1)
                printf("\nIl player avversario ha abbandonato la partita. Hai vinto!\n");
            else if(game->pid_p1 == -2)
                printf("\nIl player avversario non ha giocato nel tempo prestabilito. Hai vinto!\n");
            else
                printf("\nHai vinto la partita!\n");
        }
        else{                                                                //se sei player 1
            if(game->pid_p2 == -1)
                printf("\nIl player avversario ha abbandonato la partita. Hai vinto!\n");
            else if(game->pid_p2 == -2)
                printf("\nIl player avversario non ha giocato nel tempo prestabilito. Hai vinto!\n");
            else
                printf("\nHai vinto la partita!\n");
        }
    }
    else if(game->winner == enemy)                                                                   //se hai perso                  
        printf("\nHai perso!\n");
    else if(game->winner == 2)                                                                      //se è un pareggio
        printf("\nPareggio!\n");
    else
        return;

    closeGame();
}

void closeGame(){
    printf("\nChiusura partita in corso...\n\n");
    pthread_mutex_unlock(&game->mutex);
    shmdt(game);
    exit(0);
}