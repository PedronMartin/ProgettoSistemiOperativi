#include <stdio.h>
#include "TrisStruct.h"

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