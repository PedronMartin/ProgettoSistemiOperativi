#Makefile progetto

#compilatore
CC = gcc

#definizione sorgenti ed eseguibili
CLIENT_SRC = src/TriClient.c
SERVER_SRC = src/TriServer.c
ERREXIT_SRC = src/errorExit.c
TRISTRUCT_SRC = src/TrisStruct.c
CLIENT_OBJ = src/TriClient.o
SERVER_OBJ = src/TriServer.o
ERREXIT_OBJ = src/errorExit.o
TRISTRUCT_OBJ = src/TrisStruct.o
CLIENT_BIN = Client
SERVER_BIN = Server

#main
all: bin/$(CLIENT_BIN) bin/$(SERVER_BIN)

bin/$(CLIENT_BIN): $(CLIENT_OBJ) $(ERREXIT_OBJ) $(TRISTRUCT_OBJ)
	@$(CC) $(CLIENT_OBJ) $(ERREXIT_OBJ) $(TRISTRUCT_OBJ) -o bin/$(CLIENT_BIN)
	@echo "\nFile " $@ " pronto per l'esecuzione...\n"

# Regola per compilare il server
bin/$(SERVER_BIN): $(SERVER_OBJ) $(ERREXIT_OBJ) $(TRISTRUCT_OBJ)
	@$(CC) $(SERVER_OBJ) $(ERREXIT_OBJ) $(TRISTRUCT_OBJ) -o bin/$(SERVER_BIN)
	@echo "\nFile " $@ " pronto per l'esecuzione...\n"

$(TRISTRUCT_OBJ): $(TRISTRUCT_SRC)
	@$(CC) -Wall -I./inc $< -c -o $@
	@echo "\nFile " $@ " da " $< " creato con successo.\n"

$(ERREXIT_OBJ): $(ERREXIT_SRC)
	@$(CC) -Wall -I./inc $< -c -o $@
	@echo "\nFile " $@ " da " $< " creato con successo.\n"

$(CLIENT_OBJ): $(CLIENT_SRC)
	@$(CC) -Wall -I./inc $< -c -o $@
	@echo "\nFile " $@ " da " $< " creato con successo.\n"

$(SERVER_OBJ): $(SERVER_SRC)
	@$(CC) -Wall -I./inc $< -c -o $@
	@echo "\nFile " $@ " da " $< " creato con successo.\n"

#regola di pulizia eseguibili
.PHONY: clean

clean:
	rm -f bin/* src/*.o
