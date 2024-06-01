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
#include "errorExit.h"
#include "SignalMask.h"

//dichiaraioni delle funzioni
void checkParameters(int, char*[]);
void signalManage();
void enterSession();
void waitPlayers();
void play();
void victory();
void sigTimeout();
void sigIntManage(int);
void signalToServer(int);
void sigFromServer(int);
void closeErrorGame();
void closeGameSuccessfull();

//dichiarazione variabili globali
int shmid, semid;
char *playername;
int player;
int enemy;
int row, column;
int player2Connected = 0;
int bot = 0;
struct Tris *game;

int main(int argc, char *argv[]){

    //controllo i valori inseriti da terminale
    checkParameters(argc, argv);

    //salvo il nome del giocatore
    playername = argv[1];

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

void checkParameters(int argc, char *argv[]){
    if(argc != 2 && argc != 3){
        printf("\nFactor esecuzione errato.\nFormato richiesto: ./eseguibile <nome_utente>");
        printf("\nOppure ./eseguibile <nome_utente> '*' per giocare contro un bot.\n\n");
        exit(EXIT_FAILURE);
    }
    if(argc == 3 && *argv[2] != '*'){
        printf("\nFactor esecuzione errato.\nFormato richiesto: ./eseguibile <nome_utente>");
        printf("\nOppure ./eseguibile <nome_utente> '*' per giocare contro un bot.\n\n");
        exit(EXIT_FAILURE);
    }
    if(argc == 3 && *argv[2] == '*')
        bot = 1;
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
    if(game->pid_p1 == -1){                                             //se il primo giocatore non è ancora entrato
        game->pid_p1 = getpid();                                        //significa che sono io il primo giocatore
        player = 0;                                                     //imposto il player locale
        enemy = 1;                                                      //imposto il player avversario
        game->bot = bot;                                                //imposto se è stato chiesto di giocare con un bot
        pthread_mutex_unlock(&game->mutex);                             //esco dalla SC
        struct sembuf sops = {0, 1, 0};                                 //inizializzo la struttura per l'operazione (+1)
        if(semop(semid, &sops, 1) == -1){                               //comunichiamo al server che siamo entrati come P1
            errorExit("\nErrore nella comunicazione della connessione.\n");
            exit(EXIT_FAILURE);                                         //non siamo ancora entrati quindi uscita classica
        }
        printf("\nPlayer %s connesso.\n", playername);                  //comunico al player che è connesso in output
        printf("\nIn attesa del secondo giocatore...\n");               //comunico al player che è in attesa di P2
        struct sembuf sops2 = {0, -1, 0};                               //struttura per l'operazione (-1) per attesa di P2
        if(semop(semid, &sops2, 1) == -1){                              //attendo player 2
            if(errno != EINTR){
                errorExit("\nErrore nell'attesa del giocatore 2.\n");   //gestione errore
                closeErrorGame();
            }
        }
    }
    else{                                                               //se il primo giocatore è già entrato
        game->pid_p2 = getpid();                                        //significa che sono il secondo giocatore
        player = 1;                                                     //imposto il player locale
        enemy = 0;                                                      //imposto il player avversario
        pthread_mutex_unlock(&game->mutex);                             //esco dalla SC
        struct sembuf sops = {0, 2, 0};                                 //inizializzo la struttura per l'operazione (+2)
        if(semop(semid, &sops, 1) == -1){                               //comunichiamo a Server e P1 che siamo entrati come P2
            errorExit("\nErrore nella comunicazione della connessione.\n");
            closeErrorGame();
        }
        printf("\nPlayer %s connesso.\n", playername);                  //comunico al player che è connesso in output
    }
    player2Connected = 1;                                               //ora un eventuale abbandono verrà gestito in modo diverso
    printf("\nIl gioco può iniziare. Tu sei il player con simbolo %c\n", game->simbolo[player]);
    printf("Il tuo avversario è il player con simbolo %c\n", game->simbolo[enemy]);
}

void play(){
    struct sembuf sops1 = {player + 1, -1, 0};                          //struttura per l'operazione (-1) in base a che processo sono
    struct sembuf sops2 = {player + 1, +1, 0};                          //struttura per l'operazione (+1) in base a che processo sono
    if(player)
        printf("\nTurno del player avversario\n");                      //comunico il turno del player
    else
        printf("\nOra è il tuo turno\n\n");
    if(semop(semid, &sops1, 1) == -1){                                  //attendo che il server mi dia il via (no while perchè chiude in tutti i segnali che può ricevere)
        errorExit("\nErrore nell'attesa del turno.\n");                 //gestione errore
        closeErrorGame();
    }
    char input[10];
    while(game->winner == -1){
        printBoard(game);                                               //stampa la matrice di gioco
        int flag = 1;
        printf("\nOra è il tuo turno. Hai %d secondi per giocare.\n", game->timeout);
        do{
            flag = 1;
            printf("\nInserisci riga e colonna dove inserire il simbolo: ");
            fgets(input, sizeof(input), stdin);                         //leggo da tastiera
            if(sscanf(input, "%d %d", &row, &column) != 2){             //controllo inserimento di due interi in buffer
                printf("\nInserisci due numeri separati da uno spazio.\n");  
                                                                        //2 sono gli elementi che devono essere letti da sscanf con successo
                flag = 0;                                               //se i valori non sono numerici ripeto il ciclo
                continue;
            }
            else if(row < 0 || row > righe - 1 || column < 0 || column > colonne - 1){             
                printf("\nValori inseriti non validi.\n");
                flag = 0;                                               //se i valori non sono validi ripeto il ciclo
                continue;
            }
            else if(game->board[row][column] != -1){                    //se la casella è già occupata ripeto il ciclo
                printf("\nCasella già occupata.\n");
                flag = 0;
                continue;
            }
        }while(!flag);

        pthread_mutex_lock(&game->mutex);                               //entro in SC
        game->board[row][column] = player;                              //inserisco il simbolo nella matrice
        pthread_mutex_unlock(&game->mutex);                             //esco da SC
        printBoard(game);                                               //stampa la matrice di gioco post mossa
        if(semop(semid, &sops2, 1) == -1){                              //comunico al server che ho finito il turno
            errorExit("\nErrore nella comunicazione di fine turno\n");  //gestione errore
            closeErrorGame();
        }
        printf("\n\nTurno del player avversario\n");                    //comunico il turno del player
        if(semop(semid, &sops1, 1) == -1){                              //attendo che il server mi dia il via (no while)
            errorExit("\nErrore nell'attesa del turno.\n");             //gestione errore
            closeErrorGame();
        }
    }
    victory();                                                          //comunico il vincitore
}

void victory(){
    printf("\nTAVOLA FINALE:\n");
    printBoard(game);                                                   //stampa la matrice di gioco
    if(game->winner == player){                                         //se hai vinto
        if(player){                                                     //e sei player 2
            if(game->pid_p1 == -1)                                      //P1 ha abbandonato
                printf("\nIl player avversario ha abbandonato la partita. Hai vinto!\n");
            else if(game->pid_p1 == -2)                                 //P1 non ha giocato nel tempo prestabilito
                printf("\nIl player avversario non ha giocato nel tempo prestabilito. Hai vinto!\n");
            else                                                        //vittoria pulita
                printf("\nHai vinto la partita!\n");
        }
        else{                                                           //se sei player 1
            if(game->pid_p2 == -1)                                      //P2 ha abbandonato
                printf("\nIl player avversario ha abbandonato la partita. Hai vinto!\n");
            else if(game->pid_p2 == -2)                                 //P2 non ha giocato nel tempo prestabilito
                printf("\nIl player avversario non ha giocato nel tempo prestabilito. Hai vinto!\n");
            else                                                        //vittoria pulita
                printf("\nHai vinto la partita!\n");
        }
    }
    else if(game->winner == enemy)                                      //se hai perso                  
        printf("\nHai perso!\n");
    else                                                                //se è un pareggio
        printf("\nPareggio!\n");

    return;
}

void sigTimeout(){                                                     //se scade il tempo a uno dei due giocatori, la partita finisce
    if(game->winner != player)
        printf("\nTempo scaduto. Ricorda che hai un time di %d per giocare la tua mossa.\n", game->timeout);
    victory();                                                         //comunico il vincitore
    closeGameSuccessfull();
}

void sigIntManage(int sig){
    pthread_mutex_lock(&game->mutex);                                  //entro in SC
    if(player2Connected){
        printf("\nAbbandono partita in corso...\n");
        if(player)                                                     //in base a se sono player 2 o player 1
            game->pid_p2 = -1;
        else
            game->pid_p1 = -1;                                         //comunico abbandono a Server
    }
    else{
        printf("\nAbbandono matchmaking...\n\n");
        game->pid_p1 = -1;                                             //comunico abbandono a Server
    }

    pthread_mutex_unlock(&game->mutex);                                //esco da SC
    signalToServer(player2Connected);
    closeGameSuccessfull();
}

void signalToServer(int which){
    if(!which){                                                        //player 2 non connesso
        if(kill(game->pid_s, SIGUSR2) == -1){                          //comunico al server che abbandono il matchmaking
            errorExit("\nErrore nella comunicazione con il server.\n");
            closeErrorGame();
        }
    }
    else                                                               //player 2 connesso
        if(kill(game->pid_s, SIGUSR1) == -1){                          //comunico al server che ho abbandonato la partita avviata
            errorExit("\nErrore nella comunicazione con il server.\n");
            closeErrorGame();
        }
}

void sigFromServer(int sig){
    if(sig == SIGUSR1)                                                 //SIGUSR1 = server ha abbandonato
        printf("\nLa partita termina in modo anomalo perchè il server è caduto.\n");
    else                                                               //SIGUSR2 = partita terminata con successo
        victory();                                                     //comunico il vincitore
    closeGameSuccessfull();
}

void closeErrorGame(){
    printf("\nChiusura partita in corso generata da un errore...\n\n");
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
    printf("\nChiusura partita in corso...\n\n");
    exit(EXIT_SUCCESS);
}