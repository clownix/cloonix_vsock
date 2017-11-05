CC = gcc
AR = ar
RANLIB = ranlib
CFLAGS=-I.

all: obj/ libmdl.a

obj/:
	mkdir -p obj/


obj/mdl.o: mdl.c
	$(CC) $(CFLAGS) -c $^ -o $@

libmdl.a: obj/mdl.o
	$(AR) rc $@ obj/mdl.o
	$(RANLIB) libmdl.a

clean:
	rm -rf libmdl.a obj/
