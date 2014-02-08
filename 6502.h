/* 
 * 6502.h
 * by Max Willsey
 * a basic, clean 6502 cpu emulator 
 */

#include <stdio.h>
#incluce "memory.h"

/* 
 * ---------- type and struct definitions ---------- 
 */


typedef struct { 

  /* pointers into special locations in memory */
  byte *cartidge_upper_bank;
  byte *cartidge_lower_bank;

  /* 8-bit registers (except for PC) */
  byte A;              /* accumulator */
  byte X, Y;           /* X, Y-index registers */
  byte SP;             /* stack pointer */
  word PC;             /* program counter, the only 16 bit register */
  byte P;              /* processor status register */

} cpu;

/* initializes a cpu */
void cpu_init (cpu *c);

/* returns 0 on successful execution of a single instruction */
int cpu_step (cpu *c);
