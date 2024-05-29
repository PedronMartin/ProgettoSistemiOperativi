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
        int winner;                 //vincitore
    };
void printBoard(struct Tris* game){              //funzione per la stampa della matrice di gioco
    for(int i = 0; i < righe; i++){
        for(int j = 0; j < colonne; j++){
            if(game->board[i][j] == -1){
                printf("  ");
            }
            else if(game->board[i][j] == 0)
                printf("%c ", game->simbolo[0]);
            else
                printf("%c ", game->simbolo[1]);

            if (j < colonne - 1)                //stampa un separatore tra le celle, tranne l'ultima
                printf(" | ");
        }
        printf("\n");

        if (i < righe - 1) {                    //stampa una linea di separazione dopo ogni riga, tranne l'ultima
            for(int j = 0; j < colonne; j++){
                printf("----");                 //stampa 4 caratteri - per ogni cella
            }
            printf("\n");
        }
    }
}
#endif