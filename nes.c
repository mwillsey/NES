/* 
 * nes.c
 * by Max Willsey
 * a simple NES emulator
 */

#include "nes.h"

void nes_init (nes *n) {
  n->c = malloc(sizeof(cpu));
  n->p = malloc(sizeof(ppu));
  cpu_init(n);
  ppu_init(n);
  n->lower_bank = &n->c->mem->ram[0x8000];
  n->upper_bank = &n->c->mem->ram[0xc000];
}

void nes_step (nes *n) {                        
  int i;
  cpu_step(n);
  /* for (i = 0; i < 20; i++) */
  /*   ppu_draw_pixel(n); */
}

byte *nes_frame_buffer(nes *n) {
  return (byte*) n->p->frame_buffer;
}

void nes_destroy (nes *n) {
  cpu_destroy(n);
  ppu_destroy(n);
  free(n->c);
  free(n->p);
}
