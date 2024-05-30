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
#include <limits.h>
#include "TrisStruct.h"
#include "errorExit.h"

//dichiaraioni delle funzioni
void checkParameters(int, char*[]);
int is_number(char*);
void signalManage();
bool memoryCreation();
void boardCreation(char*[]);
void waitPlayers();
void sigP1Disconnected();
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
int playerConnected = 0;

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
        exit(EXIT_FAILURE);
    }
    int valid = is_number(argv[1]);                                    //controllo che il tempo di timeout sia un intero
    if(valid == -1){                                                   //se non è un intero
        printf("\nIl tempo di timeout della mossa deve essere un intero.\n");
        exit(EXIT_FAILURE);
    }
    if(valid < 0){                                                     //controllo che il tempo di timeout sia un intero positivo
        printf("\nIl tempo di timeout della mossa deve essere >0 (0 se non lo si vuole).\n");
        exit(EXIT_FAILURE);
    }
    char p1 = *argv[2];                                                //assegno il simbolo del player 1
    char p2 = *argv[3];                                                //assegno il simbolo del player 2
    if(p1 == p2){                                                      //controllo che i simboli dei due player siano diversi
        printf("\nI simboli dei due giocatori devono essere diversi.\n");
        exit(EXIT_FAILURE);
    }
}

int is_number(char *parameter){                       
    char *end;                                                         //puntatore a fine stringa                  
    long ris = strtol(parameter, &end, 10);                            //conversione in base 10
    if(*end != '\0' || ris > INT_MAX || ris < INT_MIN)                 //se != '\0' allora c'è un carattere non numerico
        return -1;                                                     //se > INT_MAX o < INT_MIN allora è fuori dal range di int
    return (int) ris;
}

void signalManage(){
    if(signal(SIGALRM, sigTimeout) == SIG_ERR){                        //gestione segnale per timeout
        errorExit("\nErrore nella gestione del segnale SIGALRM.\n");
        exit(EXIT_FAILURE);
    }
    //gestione del segnale Ctrl ^C prima che i giocatori siano connessi e gestione della caduta del terminale
    if(signal(SIGINT, sigIntManage) == SIG_ERR || signal(SIGHUP, sigIntManage) == SIG_ERR){
        errorExit("\nErrore nella gestione del segnale SIGINT o SIGHUP.\n");
        memoryClosing();
        exit(EXIT_FAILURE);
    }
    if(signal(SIGUSR1, checkYield) == SIG_ERR){                        //gestione segnale per abbandono di un player
        errorExit("\nErrore nella gestione del segnale SIGUSR1.\n");
        memoryClosing();
        exit(EXIT_FAILURE);
    }
    if(signal(SIGUSR2, sigP1Disconnected) == SIG_ERR){                 //gestione segnale per chiusura anomala
        errorExit("\nErrore nella gestione del segnale SIGUSR2.\n");
        memoryClosing();
        exit(EXIT_FAILURE);
    }
}

bool memoryCreation(){
    bool result = false;
    key_t key = ftok("./Server", 111);                          //creo la chiave unica per IPC
    if(key == -1) {                                                    //gestione errore
        errorExit("\nErrore nella creazione della chiave.\n");
        exit(EXIT_FAILURE);
    }

    //creo la memoria condivisa (fallisce se già esistente)
    shmid = shmget(key, sizeof(game), 0666 | IPC_CREAT | IPC_EXCL);
    if(shmid == -1 && errno == EEXIST){                                //gestione errore
        errorExit("\nErrore nella creazione della memoria condivisa.\nProbabile causa: memoria condivisa già esistente.\n");
        shmid = shmget(key, 1024, 0666);                               //accedo alla memoria condivisa già esistente
        result = false;
    }
    else if (shmid == -1 && errno != EEXIST){                          //gestione errore
        errorExit("\nErrore nella creazione della memoria condivisa.\n");
        result = false;
    }
    else
        result = true;

    semid = semget(key, 3, 0666 | IPC_CREAT | IPC_EXCL);               //creo i semafori (fallisce se già esistente)       
    if(semid == -1 && errno == EEXIST){                                //gestione errore
        errorExit("\nErrore nella creazione dei semafori.\nProbabile causa: semafori già esistenti.\n");
        semid = semget(key, 3, 0666);                                  //accedo ai semafori già esistenti
        result = false;
    }
    else if (semid == -1 && errno != EEXIST){                         //gestione errore
        errorExit("\nErrore nella creazione dei semafori.\n");
        result = false;
    }
    return result;
}

void boardCreation(char *argv[]){
    game = (struct Tris*)shmat(shmid, NULL, 0);                        //attacco la memoria condivisa al processo
    if(game == (void*)-1){                                             //gestione errore (cast per compatibilità puntatore)
        errorExit("\nErrore nell'attacco alla memoria condivisa.\n");
        memoryClosing();
        exit(EXIT_FAILURE);
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
    printf("\nAttesa del giocatore 1...\n");
    if(semctl(semid, 0, SETVAL, 0) == -1){                             //inizializzo il semaforo indice 0 in attesa dei giocatori
        errorExit("\nErrore nell'inizializzazione del semaforo 1.\n"); //gestione errore
        memoryClosing();
        exit(EXIT_FAILURE);
    }
    struct sembuf sops = {0, -1, 0};                                   //inizializzo la struttura per la wait
    while(semop(semid, &sops, 1) == -1){                               //eseguo la wait
        if(errno != EINTR){
            errorExit("\nErrore nell'attesa del giocatore 1.\n");      //gestione errore
            memoryClosing();
            exit(EXIT_FAILURE);
        }
    }
    printf("\nPlayer 1 connesso.\n");
    printf("Attesa del secondo giocatore...\n");
    playerConnected = 1;                                               //imposto il flag per comunicare a P1 un eventuale  
    while(semop(semid, &sops, 1) == -1){                               //eseguo la wait in ciclo
        if(errno != EINTR){                                            //errore segnale di interruzione
            errorExit("\nErrore nell'attesa dei giocatori.\n");        
            if(kill(game->pid_p1, SIGUSR1) == -1)                      //comunico l'abbandono del player 1
                errorExit("\nErrore nella comunicazione con il player 1.\n");
            memoryClosing();
            exit(EXIT_FAILURE);
        }
        if(errno == EINTR && playerConnected == 0){
            printf("\nPlayer 1 disconnesso durante il matchmaking.\n");
            waitPlayers();                                             //se player 1 si è sconnesso durante l'attesa, ripartiamo con l'attesa del player 1
        }
    }
    printf("\nPlayer 2 connesso.\n");
    
    if(semctl(semid, 1, SETVAL, 0) == -1){                             //inizializzo il semaforo per turno P1 a 0
        errorExit("\nErrore nell'inizializzazione del semaforo 1.\n"); //gestione errore
        memoryClosing();
        exit(EXIT_FAILURE);
    }
    if(semctl(semid, 2, SETVAL, 0) == -1){                             //inizializzo il semaforo per turno P2 a 0
        errorExit("\nErrore nell'inizializzazione del semaforo 1.\n"); //gestione errore
        memoryClosing();
        exit(EXIT_FAILURE);
    }
}

void sigP1Disconnected(){
    playerConnected = 0;                                               //re imposto la flag di connessione P1
    return;                                                            //e ritorno nell'attesa
}

void Tris(){
    while(game->winner == -1){                                         //finchè non c'è un vincitore
        struct sembuf sops_plus = {game->currentPlayer + 1, 1, 0};     //inizializzo la struttura per l'operazione (+1) per avviare i turni
        struct sembuf sops_minus = {game->currentPlayer + 1, -1, 0};   //inizializzo la struttura per l'operazione (-1) per attendere fine turni
        printf("\nTurno di Player %d.\n", game->currentPlayer + 1);
        while(semop(semid, &sops_plus, 1) == -1){                      //comunico a player che può giocare
            if(errno != EINTR){
                errorExit("\nErrore nel'avvio del turno dei giocatori.\n");
                memoryClosing();
                exit(EXIT_FAILURE);
            }
        }
        alarm(game->timeout);                                          //imposto il timeout
        while(semop(semid, &sops_minus, 1) == -1){                     //attendo che player abbia finito di giocare
            if(errno != EINTR){
                errorExit("\nErrore nell'attesa del turno dei giocatori.\n");
                memoryClosing();
                exit(EXIT_FAILURE);
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
        errorExit("\nErrore nella comunicazione con i player.\n");
    if(shmdt(game) == -1){                                             //tentativo di stacco della memoria condivisa
        errorExit("\nErrore nello stacco della memoria condivisa.\n");
        exit(EXIT_FAILURE);
    }                                                      
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
        errorExit("\nErrore nell'invio del segnale ai player.\n");

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
    if(ctrlC == 0){                                                   //se il segnale è stato ricevuto per la prima volta
        ctrlC++;                                                      //incremento il contatore
        printf("\nSegnale Ctrl ^C ricevuto. Premi di nuovo per chiudere...\n");
        return;
    }
    //se il segnale è stato ricevuto per la seconda volta
    if(playerConnected == 1)                                         //se il player 1 è connesso
        if(kill(game->pid_p1, SIGUSR1) == -1)                        //comunico l'uscita al player 1
            errorExit("\nErrore nella comunicazione con il player 1.\n");
    printf("\nChiusura del server forzata.\n");
    memoryClosing();                                                  //caso in cui i processi non sono connessi quindi non comunico
    exit(0);
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
        errorExit("\nErrore nella comunicazione di chiusura anomala ai player.\n");
    memoryClosing();
    exit(0);
}

void memoryClosing(){
    if(shmid != -1) {
        game = (struct Tris*)shmat(shmid, NULL, 0);                    //attacco la shm al processo (in alcuni percorsi non lo è già)
        if(game == (void*)-1){                                         //gestione errore (cast per compatibilità puntatore)
            errorExit("\nErrore nell'attacco alla memoria condivisa.\n");
            exit(EXIT_FAILURE);
        } 
        if(pthread_mutex_destroy(&game->mutex) == -1){                 //chiudo il mutex
            errorExit("\nErrore in chiusura mutex.\n");
            exit(EXIT_FAILURE);
        }
        if(shmdt(game) == -1){                                         //tentativo di stacco della memoria condivisa
            errorExit("\nErrore nello stacco della memoria condivisa.\n");
            exit(EXIT_FAILURE);
        }
        if(shmctl(shmid, IPC_RMID, NULL) == -1){                       //chiudo la memoria condivisa
            errorExit("\nErrore nella chiusura della memoria condivisa.\n");
            exit(EXIT_FAILURE);
        }
    }
    if(semid != -1)                                                    //chiudo i semafori
        if(semctl(semid, 0, IPC_RMID) == -1){
            errorExit("\nErrore nella chiusura dei semafori.\n");
            exit(EXIT_FAILURE);
        }

//CONTROLLARE COCN IPCS SU SHELL LA DIMENSIONE DELLA MEMORIA CONDIVISA E QUANTI PROCESSI SONO ATTUALMENTE ATTACCATI
//nel conteggio dei byte non dimenticare \0
}   