CC = gcc
CFLAGS = -Wall -g -D_FILE_OFFSET_BITS=64 $(shell pkg-config fuse3 --cflags)
LIBS = $(shell pkg-config fuse3 --libs)

all: mini_unionfs

mini_unionfs: src/mini_unionfs.c
	$(CC) $(CFLAGS) -o mini_unionfs src/mini_unionfs.c $(LIBS)

clean:
	rm -f mini_unionfs

.PHONY: all clean
