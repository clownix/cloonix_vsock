LIBMDL=../libmdl
CC = gcc
CFLAGS=-I${LIBMDL}
LIBS=-L${LIBMDL} -lmdl -lutil

all: obj/ vsock_cli

obj/:
	mkdir -p obj/

vsock_cli: obj/vsock_cli.o obj/scp_cli.o obj/x11_fwd.o
	$(CC) $^ -o $@ $(LIBS)

obj/vsock_cli.o: ./vsock_cli.c
	$(CC) $(CFLAGS) -c $^ -o $@

obj/scp_cli.o: ./scp_cli.c
	$(CC) $(CFLAGS) -c $^ -o $@

obj/x11_fwd.o: ./x11_fwd.c
	$(CC) $(CFLAGS) -c $^ -o $@

clean:
	rm -rf vsock_cli obj/

