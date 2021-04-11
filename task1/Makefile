CC     = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Og -g

all: map pointer

map: map.o
	$(CC) -o map map.o

map.o: map.c
	$(CC) -c $(CFLAGS) map.c

pointer: pointer.o
	$(CC) -o pointer pointer.o

pointer.o: pointer.c
	$(CC) -c $(CFLAGS) pointer.c

clean:
	rm -f map
	rm -f pointer
	rm -f *.o
	rm -f *~
