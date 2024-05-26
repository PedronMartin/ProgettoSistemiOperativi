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
#include <pthread.h>
#include <signal.h>
#include <sys/mman.h>
#include "TrisStruct.h"

//dichiaraioni delle funzioni
void checkParameters(int, char*[]);
bool memoryCreation();
void playerturn(int);
void boardCreation(char*[]);
void waitPlayers();
bool checkYield();
bool checkVictory();
bool checkTie();
void sigIntManage(int);
void sigIntManage2(int);
void sigTimeout(int);
void sigToPlayer1();
void sigToPlayer2();
void Tris();
void memoryClosing();

//dichiarazione variabili globali
int shmid = -1, semid = -1;
struct Tris *game;
int ctrlC = 0;
int currentSignal = 0;

int main(int argc, char *argv[]){

    //controllo il numero dei valori inseriti da terminale
    checkParameters(argc, argv);

    //gestione del segnale di timeout
    if(signal(SIGALRM, sigTimeout) == SIG_ERR){                        //gestione segnale per timeout
        printf("\nErrore nella gestione del segnale.\n");
        exit(-1);
    }

    //creazione memoria condivisa per comunicazione tra i processi
    if(!memoryCreation()){
        memoryClosing();
        exit(-1);
    }

    //gestione del segnale Ctrl ^C prima che i giocatori siano connessi
    if(signal(SIGINT, sigIntManage) == SIG_ERR || signal(SIGHUP, sigIntManage) == SIG_ERR){  //gestione segnale per chiusura del server
        printf("\nErrore nella gestione del segnale.\n");
        memoryClosing();
        exit(-1);
    }

    //gestione dei segnali
    if(signal(SIGUSR1, playerturn) == SIG_ERR){                        //gestione segnale per cambio turno (da p1 a p2)
        printf("\nErrore nella gestione del segnale.\n");
        memoryClosing();
        exit(-1);
    }
    if(signal(SIGUSR2, playerturn) == SIG_ERR){                        //gestione segnale per cambio turno (da p2 a p1)
        printf("\nErrore nella gestione del segnale.\n");
        memoryClosing();
        exit(0);
    }

    //creazione campo e variabili di gioco
    boardCreation(argv);

    //attesa dei giocatori
    waitPlayers();

    //gestione del segnale Ctrl ^C
    if(signal(SIGINT, sigIntManage2) == SIG_ERR || signal(SIGHUP, sigIntManage2) == SIG_ERR){  //gestione segnale per chiusura del server
        printf("\nErrore nella gestione del segnale.\n");
        exit(-1);
    }

    //funzione di gioco principale
    Tris();

    //chiude tutta la memoria condivisa usata
    memoryClosing();

    return 0;
}   

void checkParameters(int argc, char *argv[]){
    if(argc != 4){
        printf("\nFactor esecuzione errato.\nFormato richiesto: ./eseguibile <tempo_timeout_mossa> <simbolo_p1> <_simbolo_p2>.\n\n");
        exit(0);
    }
    char timeout = atoi(argv[1]);                                      //converto il tempo di timeout in intero
    if(timeout < 0){                                  //controllo che il tempo di timeout sia un intero positivo
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

bool memoryCreation(){
    bool result = false;
    key_t key = ftok("TriServer.c", 111);                              //creo la chiave unica per IPC
    if(key == -1) {                                                    //gestione errore
        printf("\nErrore nella creazione della chiave.\n");
        exit(0);
    }
    shmid = shmget(key, sizeof(struct Tris), 0666 | IPC_CREAT | IPC_EXCL);            //creo la memoria condivisa (fallisce se già esistente)
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
    if(semctl(semid, 0, SETVAL, 0) == -1){                             //inizializzo il semaforo a 0 (come i player attuali)
        printf("\nErrore nell'inizializzazione del semaforo 1.\n");    //gestione errore
        memoryClosing();
        exit(-1);
    }
    struct sembuf sops = {0, -1, 0};                                   //inizializzo la struttura per la wait
    if(semop(semid, &sops, 1) == -1){                                  //eseguo la wait
        printf("\nErrore nell'attesa dei giocatori.\n");               //gestione errore
        memoryClosing();
        exit(-1);
    }
    printf("\nPlayer 1 connesso.\n");
    printf("Attesa del secondo giocatore...\n");
    if(semop(semid, &sops, 1) == -1){                                   //eseguo la wait
        printf("\nErrore nell'attesa dei giocatori.\n");               //gestione errore
        memoryClosing();
        exit(-1);
    }
    printf("\nPlayer 2 connesso.\n");
}

void Tris(){
    if(semctl(semid, 1, SETVAL, 0) == -1)                              //inizializzo il semaforo a 0 (come i player attuali)
        printf("\nErrore nell'inizializzazione del semaforo 2.\n");    //gestione errore
    if(semctl(semid, 2, SETVAL, 0) == -1)                              //inizializzo il semaforo a 0 (stato server)
        printf("\nErrore nell'inizializzazione del semaforo 3.\n");    //gestione errore
    struct sembuf sops = {1, -1, 0};                                   //inizializzo la struttura per la wait
    if(semop(semid, &sops, 1) == -1)                                   //eseguo la wait
        printf("\nErrore nell'attesa che giocatore 1 sia pronto.\n");
    if(semop(semid, &sops, 1) == -1)                                   //eseguo la wait
        printf("\nErrore nell'attesa che giocatore 2 sia pronto.\n");
    game = (struct Tris*)shmat(shmid, NULL, 0);                        //attacco la memoria condivisa al processo
    printf("\nTurno di Player 1.\n");
    if(kill(game->pid_p1, SIGUSR1) == -1)                              //mando il segnale al player 1 di partire
        printf("\nErrore nell'invio del segnale al player 1.\n");
    alarm(game->timeout);                                              //imposto il timeout
    readyToReceive();                                                  //comunico ai player che sono pronti a ricevere
    while(game->winner == -1){                                         //finchè non c'è un vincitore
        pause();
        alarm(0);                                                      //resetto il timer
        if(currentSignal == SIGALRM)                                   //se il segnale è di timeout
            break;  
        if(checkYield())
            break;                                                        //controllo se qualcuno ha abbandonato
        printBoard(game);                                                  //stampa la matrice di gioco
        if(checkVictory())                                                 //controllo se c'è un vincitore (se c'è esco)
            break;
        if(checkTie())                                                      //controllo se c'è pareggio (se c'è esco)
            break;
        pthread_mutex_unlock(&game->mutex);                                //esco da SC                                     
        if(currentSignal == SIGUSR1)                                                //se il segnale è dal player 1
            sigToPlayer2();
        else if(currentSignal == SIGUSR2)                                           //se il segnale è per il player 2
            sigToPlayer1();          
        alarm(game->timeout);                                              //imposto il timeout
        readyToReceive();                                                  //comunico ai player che sono pronti a ricevere  
    }
    if(game->winner == 0 || game->winner == 1)
        printf("\nIl vincitore è il player con il simbolo %c.\n", game->simbolo[game->winner]);
    else
        printf("\nPareggio!\n");
    sigToPlayer1();
    sigToPlayer2();
    shmdt(game);                                                       //stacco la memoria condivisa
}

void readyToReceive(){
    struct sembuf sops = {2, 1, 0};                                //struttura semaforo (+1 su semaforo 3)
    if(semop(semid, &sops, 1) == -1)
        printf("\nErrore nella comunicazione ai Client di pronta ricezione.\n");
}

void playerturn(int sig){
    currentSignal = sig;
    return;
}

void sigToPlayer1(){
    game->currentPlayer = 0;                                       //cambio il player corrente
    if(kill(game->pid_p1, SIGUSR1) == -1)                          //mando il segnale al player 1
        printf("\nErrore nell'invio del segnale al player 1.\n");
    printf("\nTurno di Player 1.\n");
}

void sigToPlayer2(){
    game->currentPlayer = 1;                                       //cambio il player corrente
    if(kill(game->pid_p2, SIGUSR2) == -1)                          //mando il segnale al player 2
        printf("\nErrore nell'invio del segnale al player 2.\n");
    printf("\nTurno di Player 2.\n");
}

void sigTimeout(int sig){
    pthread_mutex_lock(&game->mutex);                                 //entro in SC
    if(kill(game->pid_p1, SIGALRM) == -1 || kill(game->pid_p2, SIGALRM) == -1)   //mando il segnale ai player
        printf("\nErrore nell'invio del segnale al player 1.\n");
    if(game->currentPlayer == 0){                                      //se il player corrente è il player 1
        printf("\nTimeout scaduto per il Player 1.\n");
        game->winner = 1;
        game->pid_p1 = -2;
    }
    else{                                                              //se il player corrente è il player 2
        printf("\nTimeout scaduto per il Player 2.\n");
        game->winner = 0;
        game->pid_p2 = -2;
    }
    currentSignal = SIGALRM;
    pthread_mutex_unlock(&game->mutex);                               //esco da SC
    return;
}

bool checkVictory(){                                                    //QUESTA FUNZIONE VA RISCRITTA: mette il tris anche se ci sono simboli vuoti
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

    if(game->winner != -1){                                           //se c'è un vincitore
        printf("\nIl vincitore è il player con il simbolo %c.\n", game->simbolo[game->winner]);
        return true;
    }
    return false;
}

bool checkYield(){
    pthread_mutex_lock(&game->mutex);                                 //entro in SC
    if(game->pid_p1 == -1){                                           //se il player 1 ha abbandonato
        printf("\nIl player 1 ha abbandonato.\n");
        game->winner = 1;                                             //il player 2 vince
        pthread_mutex_unlock(&game->mutex);                           //esco da SC
        return true;
    }
    if(game->pid_p2 == -1){                                           //se il player 2 ha abbandonato
        printf("\nIl player 2 ha abbandonato.\n");
        game->winner = 0;                                             //il player 1 vince
        pthread_mutex_unlock(&game->mutex);                           //esco da SC
        return true;
    }
    pthread_mutex_unlock(&game->mutex);                               //esco da SC
    return false;
}

bool checkTie(){
    for(int i = 0; i < righe; i++)                                    //controllo se c'è pareggio
        for(int j = 0; j < colonne; j++)
            if(game->board[i][j] == -1)
                return false;                                         //se c'è almeno una casella vuota non c'è pareggio
    pthread_mutex_lock(&game->mutex);                                 //entro in SC
    game->winner = 2;                                                 //comunico pareggio
    pthread_mutex_unlock(&game->mutex);                               //esco da SC
    return true;                                                      //se non c'è nessuna casella vuota c'è pareggio
}

void sigIntManage(int sig){
    if(ctrlC == 0){                                                   //se il segnale è stato ricevuto per la prima volta
        ctrlC++;                                                      //incremento il contatore
        printf("\nSegnale Ctrl ^C ricevuto. Premi di nuovo per chiudere...\n");
        return;
    }
    else                                                              //se il segnale è stato ricevuto per la seconda volta
        printf("\nChiusura del server forzata.\n");
    memoryClosing();
    exit(-1);
}

void sigIntManage2(int sig){
    if(sig == SIGINT){
        if(ctrlC == 0){                                                   //se il segnale è stato ricevuto per la prima volta
            ctrlC++;                                                      //incremento il contatore
            printf("\nSegnale Ctrl ^C ricevuto. Premi di nuovo per chiudere...\n");
            return;
        }
        else                                                              //se il segnale è stato ricevuto per la seconda volta
            printf("\nChiusura del server forzata.\n");
    }
    else if(sig == SIGHUP)
        printf("\nChiusura del server anomala per causa esterna. Terminale caduto.\n");

    pthread_mutex_lock(&game->mutex);                              //entro in SC
    game->pid_s = -1;                                             //comunico abbandono
    pthread_mutex_unlock(&game->mutex);                            //esco da SC
    //avviso il player 1 che la partita termina in modo anomalo
    if(kill(game->pid_p1, SIGUSR1) == -1 || kill(game->pid_p2, SIGUSR2))
        printf("\nErrore nella comunicazione con il player 1.\n");
    memoryClosing();
    exit(0);
}


void memoryClosing(){
    if(shmid != -1) {
        game = (struct Tris*)shmat(shmid, NULL, 0);                    //attacco la struct alla memoria condivisa (cast necessario) 
        game->pid_p1 = -1;                                             //comunico al server che il player 1 ha abbandonato
        game->pid_p2 = -1;                                             //comunico al server che il player 2 ha abbandonato
        if(pthread_mutex_destroy(&game->mutex) == 0)                   //chiudo il mutex
            printf("\nMutex chiuso.");
        shmdt(game);                                                   //stacco la struct dalla memoria condivisa
        if(shmctl(shmid, IPC_RMID, NULL) == 0);                        //chiudo la memoria condivisa (null è puntatore a struttura di controllo, non necessario)
            printf("\nMemoria condivisa chiusa.\n");
    }
    if(semctl(semid, 0, IPC_RMID) == 0)                                //chiudo i semafori
        printf("Semafori chiusi.\n");
}   

//CONTROLLARE COCN IPCS SU SHELL LA DIMENSIONE DELLA MEMORIA CONDIVISA E QUANTI PROCESSI SONO ATTUALMENTE ATTACCATI
//nel conteggio dei byte non dimenticare \0