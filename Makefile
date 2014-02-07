CC = gcc
CFLAGS = -Wall

all: nes

6502.o: 6502.c 6502.h
	$(CC) $(CFLAGS) -c 6502.c

nes.o: nes.c
	$(CC) $(CFLAGS) -c nes.c

nes: nes.o 6502.o

clean: 
	rm *.o

test: all
	./nes nestest.nes > my.log
	python check.py
