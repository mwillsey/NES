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
  n->p->first_write = 1;
  /* reset vblank bit */
  byte b = n->p->status;
  printf("reading 2002: getting %02x\n", b);
  n->p->status &= 0x7f;
  return b;
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
  if (n->p->first_write)
    n->p->scrollx = b;
  else
    n->p->scrolly = b;
  /* flip first_write */
  n->p->first_write = !n->p->first_write;
}  

/* Address ($2006) >> write x2 */
void wcb_2006 (nes* n, byte b) {
  /* clear then write to spot indicated by first_write latch */
  if (n->p->first_write)
    n->p->addr = (n->p->addr & 0x00ff) | (b << 8);
  else
    n->p->addr = (n->p->addr & 0xff00) | b;
  /* flip first_write */
  n->p->first_write = !n->p->first_write;
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
  printf("writing to vram \n");
  /* increment ppuaddr based on A2 */
  if (n->p->ctrl & 0x05)
    n->p->addr += 32;
  else
    n->p->addr += 1;    
}

void ppu_cycle_inc (ppu *p) {
  p->cycle++;
  if (p->cycle > 340) {
    p->cycle = 0;
    p->scanline++;
    if (p->scanline > 261) {
      p->scanline = 0;
      p->even_frame = !p->even_frame;
    }
  }
}

void ppu_read_nt (ppu *p) {
  addr nt_base_addr = 0x2000 + 0x400 * (p->ctrl & 0x4);
  addr nt_offset = (p->scanline / 8)*32 + ((p->cycle + 16) / 8);
  p->nt_entry =  mem_read(p->mem, nt_base_addr + nt_offset);
}

void ppu_read_at (ppu *p) {
  addr at_base_addr = 0x2c30 + 0x400 * (p->ctrl & 0x4);
  addr at_offset = (p->scanline / 32)*8 + ((p->cycle + 16) / 32);
  p->at_latch =  mem_read(p->mem, at_base_addr + at_offset);
}

void ppu_read_pt (ppu *p, bit hi) {
  addr pt_base_addr = ((p->ctrl >> 4) & 1) ? 0x1000 : 0x0000;
  addr pt_offset = ((addr)p->nt_entry << 4) + (p->scanline & 8);
  if (hi) 
    p->pt_latch_hi = mem_read(p->mem, pt_base_addr + pt_offset + 8);
  else 
    p->pt_latch_lo = mem_read(p->mem, pt_base_addr + pt_offset);
}

void render_run (ppu *p) {
  byte bg_palette, pixel, color;
  /* loop unit is one clock */
  while (1) {
    sem_wait(p->render_clock);
    /* find out which palette we're using */
    bg_palette = 0x3f01 + 3 * (p->at_latch >> ((p->cycle % 32 / 8) * 2));
    /* make the pixel from the bit pattern */
    pixel = ((p->pt_shift_hi >> i) << p->cycle % 8) |
      (p->pt_shift_lo >> p->cycle % 8);
    /* get the color from the palette (or universal bg color) */
    color = pixel ? mem_read(p->mem, bg_palette + pixel) : mem_read(p->mem, 0x3f00);
    /* draw it */
    p->frame_buffer[p->scanline][p->cycle] = color;
  }
}

void bg_run (ppu *p) {
  while (1) {
    for 
    switch (p->cycle % 8) {
      case 1:
        ppu_read_nt(p);
        break;
      case 3:
        ppu_read_at(p);
        break;
      case 5:
        ppu_read_pt(p, 0);
        break;
      case 7:
        ppu_read_pt(p, 1);
        /* shift and load from latches*/
        p->at_shift = (p->at_shift >> 8) | ((addr)(p->at_latch << 8));
        p->pt_shift_lo = (p->pt_shift_lo >> 8) | ((addr)(p->pt_latch_lo << 8));
        p->pt_shift_hi = (p->pt_shift_hi >> 8) | ((addr)(p->pt_latch_hi << 8));
        break;
      }

  }
}

void oam_run (ppu *p) {
  while (1) {

  }
}


void ppu_run (nes *n) {
  ppu *p = n->p;
  
  while (1) {  
    /* wait for a clock, then give it to components */
    sem_wait(p->clock);
    sem_post(p->render_clock);
    sem_post(p->bg_clock);
    sem_post(p->oam_clock);

    /* propertly increment cycle */
    p->cycle++;
    if (p->cycle > 340) {
      p->cycle = 0;
      p->scanline++;
      if (p->scanline > 261) {
        p->scanline = 0;
        p->even_frame = !p->even_frame;
      }
    }

  }

  /* clean up? */
  
}

void ppu_step (nes *n) {
  ppu* p = n->p;
  addr bg_palette;
  byte pix;
  int i;

  sem_post(p->render_clock);

  //printf("cycle: %d, scanline: %d\n",p->cycle, p->scanline);

  if (p->scanline <= 239) {
    /* visible scanlines */

    if (p->cycle == 0) {
      /* this is an idle cycle */
    }

    else if (p->cycle <= 256) {
    /*   /\* oam stuff *\/ */
    /*   if (p->cycle <= 64 && p->cycle % 2) { */
    /*     /\* clearing oam2 *\/ */
    /*     oam2[p->cycle / 4][p->cycle % 4] = 0xff; */
    /*   } else { */
    /*     /\* sprite handling *\/ */
    /*     int n = 0; */
    /*     int m = 0; */
    /*     int oam2_open = 0; */
    /*     byte b; */
        
    /*     while (oam2_open < 8) { */
    /*       b = oam1[n][0]; */
    /*       oam2[oam2_open][0] = b; */
    /*       if (0 <= p->scanline - b && p->scanline - b < 8) { */
    /*         /\* sprite is in range, take the whole thing *\/ */
    /*         oam2[oam2_open][1] = oam1[n][1]; */
    /*         oam2[oam2_open][2] = oam1[n][2]; */
    /*         oam2[oam2_open][3] = oam1[n][3]; */
    /*         oam2_open++ */
    /*       } */
          
    /*     } */
          

      i = p->cycle % 8;
      bg_palette = 0x3f01 + 3 * (p->at_latch >> ((p->cycle % 32 / 8) * 2));
      pix = ((p->pt_shift_hi >> i) << 1) | (p->pt_shift_lo >> i);
      pix = pix ? mem_read(p->mem, bg_palette + pix) : mem_read(p->mem, 0x3f00);
      p->frame_buffer[p->scanline][p->cycle] = pix;
      switch (p->cycle % 8) {
      case 1:
        ppu_read_nt(p);
        break;
      case 3:
        ppu_read_at(p);
        break;
      case 5:
        ppu_read_pt(p, 0);
        break;
      case 7:
        ppu_read_pt(p, 1);
        /* shift and load from latches*/
        p->at_shift = (p->at_shift >> 8) | ((addr)(p->at_latch << 8));
        p->pt_shift_lo = (p->pt_shift_lo >> 8) | ((addr)(p->pt_latch_lo << 8));
        p->pt_shift_hi = (p->pt_shift_hi >> 8) | ((addr)(p->pt_latch_hi << 8));
        break;
      }
    }

    else if(p->cycle <= 320) {
      /* fetch sprites for next scanline */
    }

    else if(p->cycle <= 336) {
      /* load the shift registers for next scanline */
      switch (p->cycle % 8) {
      case 1:
        ppu_read_nt(p);
        break;
      case 3:
        ppu_read_at(p);
        break;
      case 5:
        ppu_read_pt(p, 0);
        break;
      case 7:
        ppu_read_pt(p, 1);
        /* shift and load from latches*/
        p->at_shift = (p->at_shift >> 8) | ((addr)(p->at_latch << 8));
        p->pt_shift_lo = (p->pt_shift_lo >> 8) | ((addr)(p->pt_latch_lo << 8));
        p->pt_shift_hi = (p->pt_shift_hi >> 8) | ((addr)(p->pt_latch_hi << 8));
        break;
      }
    }
  }

  else if (p->scanline == 240) {
    /* visible scanline, just idle*/
  }

  else if (p->scanline <= 260) {
    /* vertical blanking lines */
    if (p->scanline == 241 && p->cycle == 0) 
      p->status |= 0x80;
  }

  else {
    /* pre-render scanline (261) */
    /* TODO: add in even odd timing? */
    p->status &= 0x7f;
  }

  ppu_cycle_inc(p);
  return;
}

void ppu_init (nes *n) {
  ppu *p = n->p;
  p->mem = malloc(sizeof(memory));
  mem_init(p->mem, 0x4000, n);
  p->scanline = 0;
  p->cycle = 0;

  p->first_write = 1;

  /* we want NMIs */
  /* p->ctrl |= 0x80; */

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

















