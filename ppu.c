/* 
 * ppu.c
 * by Max Willsey
 * PPU emulation for the NES
 */

#include "nes.h"


/* PPU I/O registers */

/* Controller ($2000) > write */
void wcb_2000 (nes* n, byte b) {
  n->p->ctrl = b; 
}

/* Mask ($2001) > write */
void wcb_2001 (nes* n, byte b) {
  n->p->mask = b; 
} 

/* Status ($2002) < read */
byte rcb_2002 (nes *n) {
  /* reset address latch */
  n->p->latch = 0;
  return n->p->status;
}

/* OAM address ($2003) > write */
void wcb_2003 (nes* n, byte b) {
  /* not supported right now */
  if  (b != 0x00)
    printf("OAMADDR is not supported right now\n");
  n->p->oam_addr = b; 
}  

/* OAM data ($2004) <> read/write (only write for now) */
void wcb_2004 (nes* n, byte b) {
  /* not supported right now */
  printf("OAMDATA is not implemented\n");
  n->p->oam_addr++;
  n->p->oam_data = b; 
}  

/* Scroll ($2005) >> write x2 */
void wcb_2005 (nes* n, byte b) {
  /* write into x/y depending on latch */
  if (n->p->latch)
    n->p->scrolly = b;
  else
    n->p->scrollx = b;
  /* flip latch */
  n->p->latch = !n->p->latch;
}  

/* Address ($2006) >> write x2 */
void wcb_2006 (nes* n, byte b) {
  /* clear then write to spot indicated by latch */
  if (n->p->latch)
    n->p->addr = (n->p->addr & 0xff00) | b;
  else
    n->p->addr = (n->p->addr & 0x00ff) | (b << 8);
  /* flip latch */
  n->p->latch = !n->p->latch;
}  

/* Data ($2007) <> read/write */
byte rcb_2007 (nes* n) {
  /* read from the address in VRAM */
  byte b = mem_read(n->p->mem, n->p->addr);
  /* increment ppuaddr based on A2 */
  if (n->p->ctrl & 0x05)
    n->p->addr += 32;
  else
    n->p->addr += 1;
  return b;
}
void wcb_2007 (nes* n, byte b) {
  /* write this to the address in VRAM */
  mem_write(n->p->mem, n->p->addr, b);
  /* increment ppuaddr based on A2 */
  if (n->p->ctrl & 0x05)
    n->p->addr += 32;
  else
    n->p->addr += 1;    
}  

void ppu_init (nes *n) {
  ppu *p = n->p;
  p->mem = malloc(sizeof(memory));
  mem_init(p->mem, 0x4000, n);
  p->latch = 0;

  /* install write callbacks in CPU address space */
  n->c->mem->write_cbs[0x2000] = &wcb_2000;
  n->c->mem->write_cbs[0x2001] = &wcb_2001;
  n->c->mem->write_cbs[0x2003] = &wcb_2003;
  n->c->mem->write_cbs[0x2004] = &wcb_2004;
  n->c->mem->write_cbs[0x2005] = &wcb_2005;
  n->c->mem->write_cbs[0x2006] = &wcb_2006;
  n->c->mem->write_cbs[0x2007] = &wcb_2007;
  /* now read callbacks */
  n->c->mem->read_cbs[0x2002] = &rcb_2002;
  n->c->mem->read_cbs[0x2007] = &rcb_2007;
}

void ppu_destroy (nes *n) {
  free(n->p->mem);
}

















