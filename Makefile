#Makefile progetto

#compilatore e opzioni di compilazione
CC = gcc
CFLAGS = -Wall -I./inc

#definizione sorgenti ed eseguibili
CLIENT_SRC = src/TriClient.c
SERVER_SRC = src/TriServer.c
ERREXIT_SRC = src/errorExit.c
TRISTRUCT_SRC = src/TrisStruct.c
TRIBOT_SRC = src/TriClientBot.c
CLIENT_OBJ = src/TriClient.o
SERVER_OBJ = src/TriServer.o
ERREXIT_OBJ = src/errorExit.o
TRISTRUCT_OBJ = src/TrisStruct.o
TRIBOT_OBJ = src/TriClientBot.o
CLIENT_BIN = Client
SERVER_BIN = Server
TRIBOT_BIN = Bot

#main
all: bin/$(CLIENT_BIN) bin/$(SERVER_BIN) bin/$(TRIBOT_BIN)

#regola di compilazione del client
bin/$(CLIENT_BIN): $(CLIENT_OBJ) $(ERREXIT_OBJ) $(TRISTRUCT_OBJ)
	@$(CC) $(CLIENT_OBJ) $(ERREXIT_OBJ) $(TRISTRUCT_OBJ) -o bin/$(CLIENT_BIN)
	@echo "\nFile " $@ " pronto per l'esecuzione...\n"

#regola di compilazione del server
bin/$(SERVER_BIN): $(SERVER_OBJ) $(ERREXIT_OBJ) $(TRISTRUCT_OBJ)
	@$(CC) $(SERVER_OBJ) $(ERREXIT_OBJ) $(TRISTRUCT_OBJ) -o bin/$(SERVER_BIN)
	@echo "\nFile " $@ " pronto per l'esecuzione...\n"

#regola di compilazione del bot
bin/$(TRIBOT_BIN): $(TRIBOT_OBJ) $(ERREXIT_OBJ) $(TRISTRUCT_OBJ)
	@$(CC) $(TRIBOT_OBJ) $(ERREXIT_OBJ) $(TRISTRUCT_OBJ) -o bin/$(TRIBOT_BIN)
	@echo "\nFile " $@ " pronto per l'esecuzione...\n"

#regole di compilazione dei file oggetto
%.o: %.c
	@$(CC) $(CFLAGS) $< -c -o $@

#regola di pulizia eseguibili
.PHONY: clean

clean:
	rm -f bin/* src/*.o
