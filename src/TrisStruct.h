#ifndef TRISSTRUCT_H
    #define righe 3
    #define colonne 3
    #define TRISSTRUCT_H
    struct Tris{                    //struttura per la gestione di gioco
        pthread_mutex_t mutex;      //mutex per la gestione della SC
        int board[righe][colonne];  //matrice di gioco
        int currentPlayer;          //giocatore corrente
        char simbolo_p[1];          //simboli giocatori         
        pid_t pid_p[1];             //pid giocatori
        int winner;                 //vincitore
    };
#endif