CC     = gcc
CFLAGS = -g -Wall -Werror -pedantic-errors
all:
	$(CC) $(CFLAGS) -o chatclient chatclient.c 
	$(CC) $(CFLAGS) -o chatserver chatserver.c 
clean:
	rm chatclient chatserver

