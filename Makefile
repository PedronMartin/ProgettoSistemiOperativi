# Makefile per compilare TriClient.c e TriServer.c

# Definisci il compilatore
CC = gcc

# Definisci i file di origine e gli eseguibili
CLIENT_SRC = src/TriClient.c
SERVER_SRC = src/TriServer.c
CLIENT_BIN = Client
SERVER_BIN = Server

# Regola predefinita
all: $(CLIENT_BIN) $(SERVER_BIN)

# Regola per compilare il client
$(CLIENT_BIN): $(CLIENT_SRC)
	$(CC) $(CLIENT_SRC) -o $(CLIENT_BIN)

# Regola per compilare il server
$(SERVER_BIN): $(SERVER_SRC)
	$(CC) $(SERVER_SRC) -o $(SERVER_BIN)

# Regola per pulire i file di output
clean:
	rm -f $(CLIENT_BIN) $(SERVER_BIN)

# Regola per forzare la ricompilazione
.PHONY: all clean
