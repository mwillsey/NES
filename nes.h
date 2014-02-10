/* 
 * nes.h
 * by Max Willsey
 * a simple nes emulator
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef uint8_t  byte;
typedef uint16_t addr;

typedef struct {
  byte *ram;
  addr *mirrors;
  void (**write_cbs)(byte);
} memory;

typedef struct { 
  memory *mem;
  /* registers */
  byte A;              /* accumulator */
  byte X, Y;           /* X, Y-index registers */
  byte SP;             /* stack pointer */
  addr PC;             /* program counter, the only 16 bit register */
  byte P;              /* processor status register */
} cpu;

typedef struct {
  memory *mem;
  /* registers */
  byte ctrl;
  byte mask;
  byte status;
  byte oam_addr;
  byte oam_data;
  byte scroll;
  byte addr;
  byte data;
} ppu;

typedef struct {
  cpu *c;
  ppu *p;
  /* special spaces in memory */
  byte *upper_bank;
} nes; 

void mem_init (memory *mem, int size);
void mem_write (memory *mem, addr a, byte b);
byte mem_read (memory *mem, addr a);
void mem_destroy (memory *mem);
void mem_mirror (memory *mem, addr start, addr end, int size);

void cpu_init (nes *n);
void cpu_step (nes *n);
void cpu_destroy (nes *n);

void ppu_init (nes *n);
void ppu_destroy (nes *n);

void nes_init(nes *n);
void nes_step(nes *n);
void nes_destroy(nes *n);

