#ifndef TRISSTRUCT_H
    #define righe 3
    #define colonne 3
    struct Tris{                    //struttura per la gestione di gioco
        pthread_mutex_t mutex;      //mutex per la gestione della SC
        int timeout;                //timeout di gioco
        int board[righe][colonne];  //matrice di gioco
        int currentPlayer;          //giocatore corrente
        char simbolo[1];            //simboli giocatori         
        pid_t pid_p1;               //pid giocatore 1
        pid_t pid_p2;               //pid giocatore 2
        pid_t pid_s;                //pid server
        int bot;                    //flag per il bot
        int winner;                 //vincitore
    };
    
    void printBoard(struct Tris* game);              //funzione per la stampa della matrice di gioco
#endif