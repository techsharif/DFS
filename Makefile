CC=gcc
CFLAGS = -std=c99 -Wall
DEPS = filesys.h

all:
	$(CC) $(CFLAGS) -o shell filesys.c shell.c

