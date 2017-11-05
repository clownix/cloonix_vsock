LIBMDL=../libmdl
CC = gcc
CFLAGS=-I${LIBMDL}
LIBS=-L${LIBMDL} -lmdl -lutil

all: obj/ vsock_cli

obj/:
	mkdir -p obj/

vsock_cli: obj/vsock_cli.o
	$(CC) $^ -o $@ $(LIBS)

obj/vsock_cli.o: ./vsock_cli.c
	$(CC) $(CFLAGS) -c $^ -o $@

clean:
	rm -rf vsock_cli obj/

