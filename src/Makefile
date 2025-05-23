CC=gcc
CFLAGS=-Wall -O2

all: server client_rx client_tx

server: server.c common.h
	$(CC) $(CFLAGS) -o server server.c

client_rx: client_rx.c common.h
	$(CC) $(CFLAGS) -o client_rx client_rx.c

client_tx: client_tx.c common.h
	$(CC) $(CFLAGS) -o client_tx client_tx.c

clean:
	rm -f server client_rx client_tx
