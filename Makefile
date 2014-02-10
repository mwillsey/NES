CC = gcc
CFLAGS = -Wall -g

all: emu

memory.o: memory.c nes.h
	$(CC) $(CFLAGS) -c memory.c

cpu.o: cpu.c nes.h
	$(CC) $(CFLAGS) -c cpu.c

ppu.o: ppu.c nes.h
	$(CC) $(CFLAGS) -c ppu.c

nes.o: nes.c nes.h
	$(CC) $(CFLAGS) -c nes.c

emu.o: emu.c
	$(CC) $(CFLAGS) -c emu.c

emu: emu.o nes.o cpu.o ppu.o memory.o

clean: 
	rm *.o

test: all
	./emu nestest.nes > my.log
	python check.py
