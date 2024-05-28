/***********************************************
*Matricola VRVR471276
*Alessandro Luca Cremasco
*Matricola VR471448
*Martin Giuseppe Pedron
*Data di realizzazione: 10/05/2024 -> 30/05/2024
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
#include <pthread.h>
#include <signal.h>
#include <sys/mman.h>
#include "TrisStruct.h"

//dichiaraioni delle funzioni
void checkParameters(int, char*[]);
void signalManage();
bool memoryCreation();
void boardCreation(char*[]);
void waitPlayers();
void Tris();
bool checkVictory();
bool checkTie();
void checkYield();
void sigTimeout(int);
void sigIntManage(int);
void sigIntManage2(int);
void sigToPlayer1();
void sigToPlayer2();
void memoryClosing();

//dichiarazione variabili globali
int shmid = -1, semid = -1;
struct Tris *game;
int ctrlC = 0;
int currentSignal = 0;

int main(int argc, char *argv[]){

    //controllo il numero dei valori inseriti da terminale
    checkParameters(argc, argv);

    //gestione dei segnali
    signalManage();

    //creazione memoria condivisa per comunicazione tra i processi
    if(!memoryCreation()){
        memoryClosing();
        exit(-1);
    }

    //creazione campo e variabili di gioco
    boardCreation(argv);

    //attesa dei giocatori
    waitPlayers();

    //cambio la gestione del segnale Ctrl ^C, in modo da comunicare ai 2 processi connessi eventuali uscite
    if(signal(SIGINT, sigIntManage2) == SIG_ERR || signal(SIGHUP, sigIntManage2) == SIG_ERR){
        printf("\nErrore nella gestione del segnale.\n");
        exit(-1);
    }

    //funzione di gioco principale
    Tris();

    //chiude tutta la memoria condivisa usata
    memoryClosing();

    //chiusura programma
    return 0;
}   

void checkParameters(int argc, char *argv[]){
    if(argc != 4){
        printf("\nFactor esecuzione errato.\nFormato richiesto: ./eseguibile <tempo_timeout_mossa> <simbolo_p1> <_simbolo_p2>.\n\n");
        exit(0);
    }
    char timeout = atoi(argv[1]);                                      //converto il tempo di timeout in intero
    if(timeout < 0){                                                   //controllo che il tempo di timeout sia un intero positivo
        printf("\nIl tempo di timeout della mossa deve essere >0 (0 se non lo si vuole).\n");
        exit(0);
    }
    char p1 = *argv[2];                                                //assegno il simbolo del player 1
    char p2 = *argv[3];                                                //assegno il simbolo del player 2
    if(p1 == p2){                                                      //controllo che i simboli dei due player siano diversi
        printf("\nI simboli dei due giocatori devono essere diversi.\n");
        exit(0);
    }
}

void signalManage(){
    if(signal(SIGALRM, sigTimeout) == SIG_ERR){                        //gestione segnale per timeout
        printf("\nErrore nella gestione del segnale.\n");
        exit(-1);
    }
    //gestione del segnale Ctrl ^C prima che i giocatori siano connessi
    if(signal(SIGINT, sigIntManage) == SIG_ERR || signal(SIGHUP, sigIntManage) == SIG_ERR){
        printf("\nErrore nella gestione del segnale.\n");
        memoryClosing();
        exit(-1);
    }
    if(signal(SIGUSR1, checkYield) == SIG_ERR){                        //gestione segnale per abbandono di un player
        printf("\nErrore nella gestione del segnale.\n");
        memoryClosing();
        exit(-1);
    }
}

bool memoryCreation(){
    bool result = false;
    key_t key = ftok("src/TriServer.c", 111);                              //creo la chiave unica per IPC
    if(key == -1) {                                                    //gestione errore
        printf("\nErrore nella creazione della chiave.\n");
        exit(0);
    }

    //creo la memoria condivisa (fallisce se già esistente)
    shmid = shmget(key, sizeof(struct Tris), 0666 | IPC_CREAT | IPC_EXCL);
    if(shmid == -1 && errno == EEXIST){                                //gestione errore
        printf("\nErrore nella creazione della memoria condivisa.\n");
        printf("Probabile causa: memoria condivisa già esistente.\n");
        shmid = shmget(key, 1024, 0666);                               //accedo alla memoria condivisa già esistente
        result = false;
    }
    else
        result = true;

    semid = semget(key, 3, 0666 | IPC_CREAT | IPC_EXCL);               //creo i semafori (fallisce se già esistente)       
    if(semid == -1 && errno == EEXIST){                                //gestione errore
        printf("\nErrore nella creazione dei semafori.\n");
        printf("Probabile causa: semafori già esistenti.\n");
        semid = semget(key, 3, 0666);                                  //accedo ai semafori già esistenti
        result = false;
    }
    return result;
}

void boardCreation(char *argv[]){
    game = (struct Tris*)shmat(shmid, NULL, 0);                        //attacco la memoria condivisa al processo
    if(game == (void*)-1){                                             //gestione errore (cast per compatibilità puntatore)
        printf("\nErrore nell'attacco alla memoria condivisa.\n");
        memoryClosing();
        exit(-1);
    }
    pthread_mutex_init(&game->mutex, NULL);                            //inizializzo il mutex
    pthread_mutex_lock(&game->mutex);                                  //entro in SC
    game->currentPlayer = 0;                                           //inizializzo il player corrente
    game->winner = -1;                                                 //inizializzo il vincitore
    game->timeout = atoi(argv[1]);                                     //assegno il timeout
    game->simbolo[0] = *argv[2];                                       //assegno il simbolo al player 1
    game->simbolo[1] = *argv[3];                                       //assegno il simbolo al player 2
    game->pid_p1 = -1;                                                 //inizializzo il pid player 1
    game->pid_p2 = -1;                                                 //inizializzo il pid player 2
    game->pid_s = getpid();                                            //assegno il pid del server
    for(int i = 0; i < righe; i++)                                     //inizializzo la matrice di gioco (valori a -1)
        for(int j = 0; j < colonne; j++)
            game->board[i][j] = -1;
    pthread_mutex_unlock(&game->mutex);                                //esco da SC
}

void waitPlayers(){
    printf("\nAttesa dei giocatori...\n");
    if(semctl(semid, 0, SETVAL, 0) == -1){                             //inizializzo il semaforo indice 0 in attesa dei giocatori
        printf("\nErrore nell'inizializzazione del semaforo 1.\n");    //gestione errore
        memoryClosing();
        exit(-1);
    }
    struct sembuf sops = {0, -1, 0};                                   //inizializzo la struttura per la wait
    if(semop(semid, &sops, 1) == -1){                                  //eseguo la wait
        if(errno != EINTR)
            printf("\nErrore nell'attesa dei giocatori.\n");               //gestione errore
        memoryClosing();
        exit(-1);
    }
    printf("\nPlayer 1 connesso.\n");
    printf("Attesa del secondo giocatore...\n");
    if(semop(semid, &sops, 1) == -1){                                  //eseguo la wait
        if(errno != EINTR)
            printf("\nErrore nell'attesa dei giocatori.\n");           //gestione errore
        if(kill(game->pid_p1, SIGUSR1) == -1)                          //comunico l'abbandono del player 1
            printf("\nErrore nella comunicazione con il player 1.\n");
        memoryClosing();
        exit(-1);
    }
    printf("\nPlayer 2 connesso.\n");
    
    if(semctl(semid, 1, SETVAL, 0) == -1){                             //inizializzo il semaforo per turno P1 a 0
        printf("\nErrore nell'inizializzazione del semaforo 1.\n");    //gestione errore
        memoryClosing();
        exit(-1);
    }
    if(semctl(semid, 2, SETVAL, 0) == -1){                             //inizializzo il semaforo per turno P2 a 0
        printf("\nErrore nell'inizializzazione del semaforo 1.\n");    //gestione errore
        memoryClosing();
        exit(-1);
    }
}

void Tris(){
    while(game->winner == -1){                                         //finchè non c'è un vincitore
        struct sembuf sops_plus = {game->currentPlayer + 1, 1, 0};         //inizializzo la struttura per l'operazione (+1) per avviare i turni
        struct sembuf sops_minus = {game->currentPlayer + 1, -1, 0};       //inizializzo la struttura per l'operazione (-1) per attendere fine turni
        printf("\nTurno di Player %d.\n", game->currentPlayer + 1);
        while(semop(semid, &sops_plus, 1) == -1){                         //comunico a player che può giocare
            if(errno != EINTR){
                printf("\nErrore nel'avvio del turno dei giocatori.\n");           //gestione errore
                memoryClosing();
                exit(0);
            }
        }
        alarm(game->timeout);                                          //imposto il timeout
        while(semop(semid, &sops_minus, 1) == -1){                        //attendo che player abbia finito di giocare
            if(errno != EINTR){
                printf("\nErrore nell'attesa dei giocatori.\n");           //gestione errore
                memoryClosing();
                exit(0);
            }
        }
        alarm(0);                                                      //resetto il timer
        printBoard(game);                                              //stampa la matrice di gioco
        if(checkVictory())                                             //controllo se c'è un vincitore (se c'è esco)
            break;
        if(checkTie())                                                 //controllo se c'è pareggio (se c'è esco)
            break;
        if(game->currentPlayer == 0)                                   //cambio il player corrente
            game->currentPlayer = 1;
        else
            game->currentPlayer = 0;
    }
    //comunico ai player la fine della partita
    if(kill(game->pid_p1, SIGUSR2) == -1 || kill(game->pid_p2, SIGUSR2) == -1)
        printf("\nErrore nella comunicazione con i player.\n");
    shmdt(game);                                                       //stacco la memoria condivisa
    return;
}

bool checkVictory(){
    pthread_mutex_lock(&game->mutex);                                  //entro in SC
    for(int i = 0; i < righe; i++)                                     //controllo righe
        if(game->board[i][0] == game->board[i][1] && game->board[i][1] == game->board[i][2] && game->board[i][0] != -1)
            game->winner = game->board[i][0];
    for(int i = 0; i < colonne; i++)                                   //controllo colonne
        if(game->board[0][i] == game->board[1][i] && game->board[1][i] == game->board[2][i] && game->board[0][i] != -1)
            game->winner = game->board[0][i];
                                                                       //controllo diagonali
    if(game->board[0][0] == game->board[1][1] && game->board[1][1] == game->board[2][2] && game->board[0][0] != -1)
        game->winner = game->board[0][0];
    if(game->board[0][2] == game->board[1][1] && game->board[1][1] == game->board[2][0] && game->board[0][2] != -1)
        game->winner = game->board[0][2];

    pthread_mutex_unlock(&game->mutex);                                //esco da SC
    if(game->winner != -1){                                            //se c'è un vincitore
        printf("\nIl vincitore è il player con il simbolo %c.\n", game->simbolo[game->winner]);
        return true;
    }
    return false;
}

bool checkTie(){
    pthread_mutex_lock(&game->mutex);                                  //entro in SC
    for(int i = 0; i < righe; i++)                                     //controllo se c'è pareggio
        for(int j = 0; j < colonne; j++)
            if(game->board[i][j] == -1){
                pthread_mutex_unlock(&game->mutex);                    //esco da SC
                return false;                                          //se c'è almeno una casella vuota non c'è pareggio
            }
    game->winner = 2;                                                  //altrimenti, salvo pareggio
    pthread_mutex_unlock(&game->mutex);                                //esco da SC
    printf("\nPareggio.\n");
    return true;                                                       //comunico pareggio
}

void sigTimeout(int sig){
    pthread_mutex_lock(&game->mutex);                                  //entro in SC
                                                                       //mando il segnale ai player
    if(kill(game->pid_p1, SIGALRM) == -1 || kill(game->pid_p2, SIGALRM) == -1)
        printf("\nErrore nell'invio del segnale al player 1.\n");

    if(game->currentPlayer == 0){                                      //se il player corrente è il player 1
        printf("\nTimeout scaduto per il Player 1.\n");
        game->winner = 1;                                              //il player 2 vince
        game->pid_p1 = -2;                                             //comunico sconfitta per timeout
    }
    else{                                                              //se il player corrente è il player 2
        printf("\nTimeout scaduto per il Player 2.\n");
        game->winner = 0;                                              //il player 1 vince
        game->pid_p2 = -2;                                             //comunico sconfitta per timeout
    }
    pthread_mutex_unlock(&game->mutex);                                //esco da SC
    memoryClosing();
    exit(0);
}

void checkYield(){
    pthread_mutex_lock(&game->mutex);                                 //entro in SC
    if(game->pid_p1 == -1){                                           //se il player 1 ha abbandonato
        printf("\nIl player 1 ha abbandonato.\n");
        game->winner = 1;                                             //il player 2 vince
        if(kill(game->pid_p2, SIGUSR2) == -1)                         //comunico vittoria per abbandono
            printf("\nErrore nella comunicazione con il player 2.\n");
    }
    else{
        printf("\nIl player 2 ha abbandonato.\n");
        game->winner = 0;                                             //il player 1 vince
        if(kill(game->pid_p1, SIGUSR2) == -1)                         //comunico vittoria per abbandono
            printf("\nErrore nella comunicazione con il player 1.\n");
    }
    pthread_mutex_unlock(&game->mutex);                               //esco da SC
    memoryClosing();
    exit(0);
}

void sigIntManage(int sig){
    /*
    if(ctrlC == 0){                                                   //se il segnale è stato ricevuto per la prima volta
        ctrlC++;                                                      //incremento il contatore
        printf("\nSegnale Ctrl ^C ricevuto. Premi di nuovo per chiudere...\n");
        return;
    }
    else                                                              //se il segnale è stato ricevuto per la seconda volta
    */ //chiedere a Drago se va bene
    printf("\nChiusura del server forzata.\n");
    memoryClosing();                                                  //caso in cui i processi non sono connessi quindi non comunico
    exit(-1);
}

void sigIntManage2(int sig){
    if(sig == SIGINT){
        if(ctrlC == 0){                                               //se il segnale è stato ricevuto per la prima volta
            ctrlC++;                                                  //incremento il contatore
            printf("\nSegnale Ctrl ^C ricevuto. Premi di nuovo per chiudere...\n");
            return;
        }
        else                                                          //se il segnale è stato ricevuto per la seconda volta
            printf("\nChiusura del server forzata.\n");
    }
    else if(sig == SIGHUP)                                            //terminale caduto
        printf("\nChiusura del server anomala per causa esterna. Terminale caduto.\n");

    game->pid_s = -1;                                                 //salva abbandono
                                                                      //avviso i player che la partita termina in modo anomalo
    if(kill(game->pid_p1, SIGUSR1) == -1 || kill(game->pid_p2, SIGUSR1))
        printf("\nErrore nella comunicazione di chiusura anomala ai player.\n");
    memoryClosing();
    exit(0);
}

void memoryClosing(){
    if(shmid != -1) {
        game = (struct Tris*)shmat(shmid, NULL, 0);                    //attacco la shm al processo (in alcuni percorsi non lo è già) 
        if(pthread_mutex_destroy(&game->mutex) == 0)                   //chiudo il mutex
            printf("\nMutex chiuso.");
        shmdt(game);                                                   //stacco la struct dalla memoria condivisa
        if(shmctl(shmid, IPC_RMID, NULL) == 0);                        //chiudo la memoria condivisa (null è puntatore a struttura di controllo, non necessario)
            printf("\nMemoria condivisa chiusa.\n");
    }
    if(semctl(semid, 0, IPC_RMID) == 0)                                //chiudo i semafori
        printf("Semafori chiusi.\n");

//CONTROLLARE COCN IPCS SU SHELL LA DIMENSIONE DELLA MEMORIA CONDIVISA E QUANTI PROCESSI SONO ATTUALMENTE ATTACCATI
//nel conteggio dei byte non dimenticare \0
}   