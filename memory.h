/* 
 * memory.h
 * by Max Willsey
 * A way to emulate a memory bank with mirroring 
 */

#include <stdint.h>
#include <stdlib.h>

typedef uint8_t  byte;
typedef uint16_t addr;

typedef struct {
  byte *ram;
  addr *mirrors;
  byte (**read_cbs)(addr);
  void (**write_cbs)(addr, byte);
} memory;

void mem_init (memory *mem, int size);
void mem_write (memory *mem, addr a, byte b);
byte mem_read (memory *mem, addr a);
void mem_destroy (memory *mem);

/* a convenience function used to set up memory mirroring */
void mem_mirror (memory *mem, addr start, addr end, int size);



















