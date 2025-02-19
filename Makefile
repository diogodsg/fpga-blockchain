CC = gcc
CFLAGS = -Wall -Wextra -g
LIBS = -lcrypto -lmicrohttpd

all: blockchain

blockchain: main.c
	$(CC) $(CFLAGS) main.c -o blockchain $(LIBS)

clean:
	rm -f blockchain
