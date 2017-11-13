LIBMDL=../libmdl
CC = gcc
CFLAGS=-I${LIBMDL}
LIBS=-L${LIBMDL} -lmdl -lutil


all: obj/ vsock_srv

obj/:
	mkdir -p obj/

vsock_srv: obj/vsock_srv.o obj/scp_srv.o obj/x11_fwd.o
	$(CC) $^ -o $@ $(LIBS)

obj/vsock_srv.o: ./vsock_srv.c
	$(CC) $(CFLAGS) -c $^ -o $@

obj/scp_srv.o: ./scp_srv.c
	$(CC) $(CFLAGS) -c $^ -o $@

obj/x11_fwd.o: ./x11_fwd.c
	$(CC) $(CFLAGS) -c $^ -o $@

clean:
	rm -rf vsock_srv obj/

