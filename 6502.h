/* 
 * 6502.h
 * by Max Willsey
 * a basic, clean 6502 cpu emulator 
 */

#include <stdio.h>
#include "memory.h"

/* 
 * ---------- type and struct definitions ---------- 
 */


typedef struct { 
  /* link to memory */
  memory *mem;
  /* 8-bit registers (except for PC) */
  byte A;              /* accumulator */
  byte X, Y;           /* X, Y-index registers */
  byte SP;             /* stack pointer */
  addr PC;             /* program counter, the only 16 bit register */
  byte P;              /* processor status register */

} cpu;

/* initializes a cpu */
void cpu_init (cpu *c, memory *mem);

/* returns 0 on successful execution of a single instruction */
int cpu_step (cpu *c);










