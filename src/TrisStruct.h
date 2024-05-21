#ifndef TRISSTRUCT_H
    #define righe 3
    #define colonne 3
    struct Tris{                    //struttura per la gestione di gioco
        pthread_mutex_t mutex;      //mutex per la gestione della SC
        int timeout;                //timeout di gioco
        int board[righe][colonne];  //matrice di gioco
        int currentPlayer;          //giocatore corrente
        char simbolo[1];            //simboli giocatori         
        pid_t pid_p[1];             //pid giocatori
        pid_t pid_s;                //pid server
        int winner;                 //vincitore
    };
    void printBoard(struct Tris* game){              //funzione per la stampa della matrice di gioco
        for(int i = 0; i < righe; i++){
            for(int j = 0; j < colonne; j++){
                if(game->board[i][j] == -1){
                    printf(" ");
                }
                else if(game->board[i][j] == 0)
                    printf("%c", game->simbolo[0]);
                else
                    printf("%c", game->simbolo[1]);
            }
            printf("\n");
        }
    }
#endif