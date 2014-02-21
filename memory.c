/* 
 * memory.c
 * by Max Willsey
 * A way to emulate a memory bank with mirroring 
 */

#include "nes.h"

void mem_init (memory *mem, int size, nes *n) {
  int a; //need a to be int so it will go over 0xffff
  mem->ram = malloc(sizeof(byte) * size);
  mem->mirrors = malloc(sizeof(addr) * size);
  mem->write_cbs = calloc(sizeof(void*) , size);
  mem->read_cbs = calloc(sizeof(void*) , size);
  
  for (a = 0; a < size; a++)
    mem->mirrors[a] = a;

  mem->n = n;
}

void mem_write (memory *mem, addr a, byte b) {
  addr a1 = mem->mirrors[a];
  mem->ram[a1] = b;
  if (mem->write_cbs[a1])
    (mem->write_cbs[a1])(mem->n, b);
}

byte mem_read (memory *mem, addr a) {
  addr a1 = mem->mirrors[a];
  if (mem->read_cbs[a1]) 
    return (mem->read_cbs[a1])(mem->n);
  return mem->ram[a1];
}

void mem_destroy (memory *mem) {
  free(mem->ram);
  free(mem->write_cbs);
}

void mem_mirror (memory *mem, addr start, addr end, int size) {
  addr a;
  for (a = start; a <= end; a++) {
    mem->mirrors[a] = a % size;
  }
}
    


















