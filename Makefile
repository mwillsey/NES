CC = gcc
CFLAGS = -Wall -g

all: nes

memory.o: memory.c memory.h
	$(CC) $(CFLAGS) -c memory.c

cpu.o: cpu.c cpu.h
	$(CC) $(CFLAGS) -c cpu.c

nes.o: nes.c
	$(CC) $(CFLAGS) -c nes.c

nes: nes.o cpu.o memory.o

clean: 
	rm *.o

test: all
	./nes nestest.nes > my.log
	python check.py
