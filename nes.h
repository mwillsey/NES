/* 
 * nes.h
 * by Max Willsey
 * a simple nes emulator
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

typedef uint8_t  byte;
typedef uint16_t addr;
typedef bool     bit;

struct cpu_s;
struct ppu_s;

typedef struct {
  struct cpu_s *c;
  struct ppu_s *p;
  /* special spaces in memory */
  byte *lower_bank;
  byte *upper_bank;
  byte *chr_rom;
} nes; 

typedef struct {
  nes *n;
  byte *ram;
  addr *mirrors;
  /* eventually I'd like to clean up the callback structure */
  void (**write_cbs)(nes*, byte);
  byte (**read_cbs) (nes*);
} memory;

struct cpu_s { 
  memory *mem;
  /* registers */
  byte A;              /* accumulator */
  byte X, Y;           /* X, Y-index registers */
  byte SP;             /* stack pointer */
  addr PC;             /* program counter, the only 16 bit register */
  byte P;              /* processor status register */
};

struct ppu_s {
  memory *mem;

  int cycle;                    /* 341 per scanline */
  int scanline;                 /* 262 per frame */
  bit even_frame;

  bit first_write;
  bit rendering;


  /*** background stuff ***/

  /* latches */
  byte nt_entry, at_entry;
  byte at_latch;
  byte pt_latch_lo, pt_latch_hi;

  /* shift register */
  addr at_shift;
  addr pt_shift_lo, pt_shift_hi;

  /*** sprite stuff ***/
  

  /* registers */
  byte ctrl;
  byte mask;
  byte status;
  byte oam_addr;
  byte oam_data;
  byte scrollx, scrolly;
  addr addr;
  /* for output */
  byte frame_buffer[240][256];
};

typedef struct cpu_s cpu;
typedef struct ppu_s ppu;

void nes_init(nes *n);
void nes_step(nes *n);
byte* nes_frame_buffer(nes *n);
void nes_destroy(nes *n);

void cpu_init (nes *n);
void cpu_load (nes *n);
void cpu_step (nes *n);
void cpu_destroy (nes *n);

void ppu_init (nes *n);
void ppu_step (nes *n);
void ppu_destroy (nes *n);

void mem_init (memory *mem, int size, nes *n);
void mem_write (memory *mem, addr a, byte b);
byte mem_read (memory *mem, addr a);
void mem_destroy (memory *mem);
void mem_mirror (memory *mem, addr start, addr end, int size);
