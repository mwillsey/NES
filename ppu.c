/* 
 * ppu.c
 * by Max Willsey
 * PPU emulation for the NES
 */

#include "nes.h"

void ctrl_reg_cb() {
}  

void ppu_init (nes *n) {
  ppu *p = n->p;
  p->mem = malloc(sizeof(memory));
  mem_init(p->mem, 0x4000, n);
}

void ppu_destroy (nes *n) {
  free(n->p->mem);
}

















