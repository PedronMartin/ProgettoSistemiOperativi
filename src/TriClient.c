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
#include "TrisStruct.h"

//dichiaraioni delle funzioni
void checkParameters(int, char*[]);
void signalManage();
void enterSession();
void waitPlayers();
void play();
void victory();
void sigTimeout();
void sigIntManage(int);
void signalToServer();
void sigFromServer(int);
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

    //gestione dei segnali
    signalManage();

    //comunica al server che sono entrato
    enterSession();

    //accedo alla memoria condivisa e attendo un avversario
    waitPlayers();

    //funzione di gioco principale
    play();

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

void signalManage(){
    if(signal(SIGALRM, sigTimeout) == SIG_ERR){                         //gestione del segnale SIGALRM
        printf("\nErrore nella gestione del segnale SIGALRM.\n");
        exit(0);
    }
                                                                        //gestione del segnale SIGINT e SIGHUP
    if(signal(SIGINT, sigIntManage) == SIG_ERR || signal(SIGHUP, sigIntManage) == SIG_ERR){
        printf("\nErrore nella gestione del segnale SIGINT.\n");
        exit(0);
    }
                                                                        //gestione del segnale SIGUSR1 e SIGUSR2
    if(signal(SIGUSR1, sigFromServer) == SIG_ERR || signal(SIGUSR2, sigFromServer) == SIG_ERR){
        printf("\nErrore nella gestione del segnale SIGUSR1.\n");
        exit(0);
    }
}

void enterSession(){
    key_t key = ftok("src/TriServer.c", 111);                               //accedo ai semafori creati dal server
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
        struct sembuf sops = {0, 1, 0};                                 //inizializzo la struttura per l'operazione (+1)
        if(semop(semid, &sops, 1) == -1){                               //comunichiamo al server che siamo entrati come P1
            printf("\nErrore nell'attesa dei giocatori.\n");            //gestione errore
            closeGame();
        }
        printf("\nPlayer %s connesso.\n", playername);                  //comunico al player che è connesso
        printf("\nIn attesa del secondo giocatore...\n");               //comunico al player che è in attesa di P2
        struct sembuf sops2 = {0, -1, 0};                               //cambio la struttura per l'operazione (-1) per attesa di P2
        if(semop(semid, &sops2, 1) == -1){                              //attendo player 2
            if(errno != EINTR)
                printf("\nErrore nell'attesa dei giocatori.\n");            //gestione errore
            closeGame();
        }
    }
    else{                                                               //se il primo giocatore è già entrato
        game->pid_p2 = getpid();                                        //significa che sono il secondo giocatore
        player = 1;                                                     //imposto il player locale
        enemy = 0;                                                      //imposto il player avversario
        pthread_mutex_unlock(&game->mutex);                             //esco dalla SC
        struct sembuf sops = {0, 2, 0};                                 //inizializzo la struttura per l'operazione (+2)
        if(semop(semid, &sops, 1) == -1){                               //comunichiamo a Server e P1 che siamo entrati come P2
            printf("\nErrore nell'attesa dei giocatori.\n");            //gestione errore
            closeGame();
        }
        printf("\nPlayer %s connesso.\n", playername);                  //comunico al player che è connesso
    }
    printf("\nIl gioco può iniziare. Tu sei il player con simbolo %c\n", game->simbolo[player]);
    printf("Il tuo avversario è il player con simbolo %c\n", game->simbolo[enemy]);
}

void play(){
    struct sembuf sops1 = {player + 1, -1, 0};                           //struttura per l'operazione (-1) in base a che processo sono
    struct sembuf sops2 = {player + 1, +1, 0};                           //struttura per l'operazione (+1) in base a che processo sono
    if(player)
        printf("\nTurno del player avversario\n");                       //comunico il turno del player
    else
        printf("\nOra è il tuo turno\n\n");
    if(semop(semid, &sops1, 1) == -1){                                   //attendo che il server mi dia il via
            printf("\nErrore nell'attesa del turno.\n");                 //gestione errore
            closeGame();
    }
    while(game->winner == -1){
        printBoard(game);                                                //stampa la matrice di gioco
        int flag = 1;
        printf("\nOra è il tuo turno. Hai %d secondi per giocare.\n", game->timeout);
        do{
            flag = 1;
            printf("\nInserisci riga e colonna dove inserire il simbolo: ");
            scanf("%d %d", &row, &column);                               //inserimento riga e colonna
            if(row < 0 || row > righe - 1 || column < 0 || column > colonne - 1){             
                printf("\nValori inseriti non validi.\n");
                flag = 0;                                                //se i valori non sono validi ripeto il ciclo
            }
            else if(game->board[row][column] != -1){                     //se la casella è già occupata ripeto il ciclo
                printf("\nCasella già occupata.\n");
                flag = 0;
            }
        }while(!flag);

        pthread_mutex_lock(&game->mutex);                                //entro in SC
        game->board[row][column] = player;                               //inserisco il simbolo nella matrice
        printBoard(game);                                                //stampa la matrice di gioco post mossa
        pthread_mutex_unlock(&game->mutex);                              //esco da SC
        if(semop(semid, &sops2, 1) == -1){                               //comunico al server che ho finito il turno
            printf("\nErrore nella comunicazione di fine turno\n");      //gestione errore
            closeGame();
        }
        printf("\n\nTurno del player avversario\n");                     //comunico il turno del player
        if(semop(semid, &sops1, 1) == -1){                               //attendo che il server mi dia il via
            printf("\nErrore nell'attesa del turno.\n");                 //gestione errore
            closeGame();
        }
    }
    victory();                                                           //comunico il vincitore
}

void victory(){
    if(game->winner == player){                                          //se hai vinto
        if(player){                                                      //e sei player 2
            if(game->pid_p1 == -1)                                       //P1 ha abbandonato
                printf("\nIl player avversario ha abbandonato la partita. Hai vinto!\n");
            else if(game->pid_p1 == -2)                                  //P1 non ha giocato nel tempo prestabilito
                printf("\nIl player avversario non ha giocato nel tempo prestabilito. Hai vinto!\n");
            else                                                         //vittoria pulita
                printf("\nHai vinto la partita!\n");
        }
        else{                                                            //se sei player 1
            if(game->pid_p2 == -1)                                       //P2 ha abbandonato
                printf("\nIl player avversario ha abbandonato la partita. Hai vinto!\n");
            else if(game->pid_p2 == -2)                                  //P2 non ha giocato nel tempo prestabilito
                printf("\nIl player avversario non ha giocato nel tempo prestabilito. Hai vinto!\n");
            else                                                         //vittoria pulita
                printf("\nHai vinto la partita!\n");
        }
    }
    else if(game->winner == enemy)                                       //se hai perso                  
        printf("\nHai perso!\n");
    else                                                                 //se è un pareggio
        printf("\nPareggio!\n");

    return;
}

void sigTimeout(){                                                      //se scade il tempo a uno dei due giocatori, la partita finisce
    if(game->winner != player)
        printf("\nTempo scaduto. Ricorda che hai un time di %d per giocare la tua mossa.\n", game->timeout);
    victory();                                                          //comunico il vincitore
    closeGame();                                                        //chiudo la partita
}

void sigIntManage(int sig){
    printf("\nAbbandono partita in corso...\n");
    if(player)                                                          //in base a se sono player 2 o player 1
        game->pid_p2 = -1;
    else
        game->pid_p1 = -1;                                              //comunico abbandono a Server
    signalToServer();                                                   //comunico al server. Non importa il tipo di segnale (SIGUSR1 o SIGUSR2)
    closeGame();
}

void signalToServer(){
    if(kill(game->pid_s, SIGUSR1) == -1)                                //comunico al server che ho abbandonato la partita
        printf("\nErrore nella comunicazione con il server.\n");
}

void sigFromServer(int sig){
    if(sig == SIGUSR1)                                                  //SIGUSR1 = server abbandona
        printf("\nLa partita termina in modo anomalo perchè il server è caduto.\n");
    else                                                                //SIGUSR2 = avversario ha abbandonato
        victory();                                                      //comunico il vincitore
    closeGame();
}

void closeGame(){
    printf("\nChiusura partita in corso...\n\n");
    shmdt(game);
    exit(0);
}