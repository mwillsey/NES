/* 
 * memory.h
 * by Max Willsey
 * A way to emulate a memory bank with mirroring 
 */

#include <stdint.h>

typedef uint8_t  byte;
typedef uint16_t addr;

typedef struct {
  byte *arr;
} memory;

void mem_init (memory mem, size_t size);
void mem_write (memory mem, addr a, byte b);
byte mem_read (memory mem, addr a);
void mem_destroy (memory mem);
