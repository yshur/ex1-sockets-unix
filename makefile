all: selectserver client uds
	gcc selectserver.o -o selectserver
	gcc client.o -o client
	gcc uds.o -o uds

selectserver: selectserver.c
	gcc -c selectserver.c -o selectserver.o

client: client.c
	gcc -c client.c -o client.o

uds: uds.c
	gcc -c uds.c -o uds.o
clean:
	rm -f selectserver client uds *.o
