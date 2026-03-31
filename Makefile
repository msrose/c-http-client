CC=cc
CFLAGS=-Wall -Wextra -pedantic

client: client.c
	$(CC) $(CFLAGS) client.c -o client
