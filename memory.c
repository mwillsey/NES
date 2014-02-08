/* 
 * memory.h
 * by Max Willsey
 * A way to emulate a memory bank with mirroring 
 */

#include "memory.h"

void mem_init (memory mem, size_t size) {
  mem->arr = malloc(sizeof(byte)*size);
}

void mem_write (memory mem, addr a, byte b) {
  mem->arr[a] = b;
}

byte mem_read (memory mem, addr a) {
  return mem_arr[a];
}

void mem_destroy (memory mem) {
  free(mem->arr);
}



















