/* 
 * 6502.c
 * by Max Willsey
 * a basic, clean 6502 cpu emulator 
 */

#include "6502.h"

/* 
 * ---------- stack functions ----------
 */

/* use these wrappers because the stack is from 0x01ff to 0x100 */

void push_byte (cpu *c, byte b) {
  mem_write(c->mem, 0x100 + c->SP--, b);
}

byte pull_byte (cpu *c) {
  return mem_read(c->mem, 0x100 + ++(c->SP));
}

void push_word (cpu *c, addr a) {
  push_byte(c, (byte)(a >> 8));
  push_byte(c, (byte)(a));
}

addr pull_word (cpu *c) {
  byte lo, hi;
  lo = pull_byte(c);
  hi = pull_byte(c);
  return ((addr)(hi) << 8) | lo;
}


/* 
 * ---------- processor status functions ----------
 */


/* bits numbered from right to left */
enum flag {
  C = 0,                        /* carry flag */
  Z = 1,                        /* zero flag */
  I = 2,                        /* interrupt disable flag */
  D = 3,                        /* decimal flag */
  B = 4,                        /* breakpoint flag */
  /* 5 bit is always 1 */
  V = 6,                        /* overflow flag */
  N = 7                         /* negative/sign flag */
};
  
/* get the desired flag  */
int get_flag (cpu *c, enum flag n) { 
  return (c->P >> n) & 0x1; 
}

/* set the desired flag accoriding to some bool */
void set_flag (cpu *c, enum flag n, int b) {
  if (b)
    c->P |= 0x1 << n;
  else
    c->P &= ~(0x1 << n);
}

/* shortcut to set zero and negative flag based on a byte */
void set_zn_flags (cpu *c, byte b) {
  set_flag(c, Z, !b);
  set_flag(c, N, b & 0x80);
}


/* 
 * ---------- addressing modes ----------
 */


/* addressing will be handled by handing every instruction the address
 * of the byte it will be operating on. That way intructions can be
 * coded without knowledge of their addressing mode. Addressing will also
 * handle incrementing the PC according to the number of arguments */

/* accumulator
 * a few operations can work on the accumulator or memory, those have bee
 * split into 2 separate functions, where the accumulator take 1 less arg.
 */

/* immediate
 * the constant immediately follows in instruction in memory 
 * we will work with the invariant that the PC is incremented until 
 * the instruction is complete
 */
addr am_imm (cpu *c) { 
  return c->PC++; 
}

/* zero page 
 * only an 8-bit address operand is given, so only the first 0x100 (256) 
 * bytes of memory can be addressed (0x0000 - 0x00FF)
 */
addr am_zer (cpu *c) { 
  return mem_read(c->mem, c->PC++);
}

/* zero page,x
 * address to be accessed is the 8-bit value given in the instruction 
 * summed with the X register
 */
addr am_zex (cpu *c) {
  return (byte)(mem_read(c->mem, c->PC++) + c->X);
}

/* zero page,y
 * address to be accessed is the 8-bit value given in the instruction 
 * summed with the X register
 */
addr am_zey (cpu *c) {
  return (byte)(mem_read(c->mem, c->PC++) + c->Y);
}

/* relative
 * addressing is identical to immediate, just handled in a signed manner
 * by the branch instructions
 */
addr am_rel (cpu *c) { 
  return c->PC++; 
}

/* absolute
 * full address to be operated on is given 2 bytes following instruction 
 * don't forget the 6502 is least significant byte first
 */
addr am_abs (cpu *c) {
  byte lo, hi;
  lo = mem_read(c->mem, c->PC++);
  hi = mem_read(c->mem, c->PC++);
  return ((addr)(hi) << 8) | lo;
}

/* absolute,x
 * uses the full address given in the 2 bytes following instruction, but
 * also adds the X register
 */
addr am_abx (cpu *c) {
  byte lo, hi;
  lo = mem_read(c->mem, c->PC++);
  hi = mem_read(c->mem, c->PC++);
  return (((addr)(hi) << 8) | lo) + c->X;
}

/* absolute,y
 * uses the full address given in the 2 bytes following instruction, but
 * also adds the Y register
 */
addr am_aby (cpu *c) {
  byte lo, hi;
  lo = mem_read(c->mem, c->PC++);
  hi = mem_read(c->mem, c->PC++);
  return (((addr)(hi) << 8) | lo) + c->Y;
}

/* indirect
 * only JMP uses indirect addressing, where the 2 byte following the 
 * instruction yield and address containing the lowest byte of the
 * address you want. For our purposes, this is identical to absolute 
 * addressing
 */
addr am_ind (cpu *c) {
  byte lo, hi, lo2, hi2;
  addr a, a1;
  lo = mem_read(c->mem, c->PC++);
  hi = mem_read(c->mem, c->PC++);
  a = ((addr)(hi) << 8) | lo;
  /* a1 is a + 1 in most cases, except to represnt a bug in the hardware 
   * where it really only adds 1 to the lo byte*/
  a1 = ((addr)(hi) << 8) | (byte)(lo + 1);
  /* now start getting the target address */
  lo2 = mem_read(c->mem, a);
  hi2 = mem_read(c->mem, a1);
  return ((addr)(hi2) << 8) | lo2;
} 

/* indexed indirect
 * easy if you imaging an address table in zero page memory. The byte
 * following the instruction gives the base address, and the index is in 
 * the X register. That will contain the LSB of the target address
 */
addr am_inx (cpu *c) {
  byte base, lo, hi;
  base = mem_read(c->mem, c->PC++);
  lo = base + c->X;
  hi = base + c->X + 1;
  return ((addr)mem_read(c->mem, hi) << 8) | mem_read(c->mem, lo);
}

/* indirect indexed
 * byte following instruction is the zero page address of the LSB of a 16-bit
 * address. Then add the Y-register to that
 */
addr am_iny (cpu *c) {
  byte z, lo, hi;
  z = mem_read(c->mem, c->PC++);
  lo = z;
  hi = z + 1;
  return (((addr)(mem_read(c->mem, hi)) << 8) | mem_read(c->mem, lo)) + c->Y;
}


/* 
 * ---------- instructions ----------
 */

/* all of these will take the a pointer to cpu struct and the location
 * in memory to be operated on. Those with implied operands wil not have
 * a second parameter. These will not affect the PC (except branches).
 * when an instruction is run, you are guaranteed the PC is on the next
 * instruction. */

/* 
 * ----- load/store operations -----
 */

/* load accumulator */
void LDA (cpu *c, addr a) {
  c->A = mem_read(c->mem, a);
  set_zn_flags(c, c->A);
}

/* load X register */
void LDX (cpu *c, addr a) {
  c->X = mem_read(c->mem, a);
  set_zn_flags(c, c->X);
}

/* load Y register */
void LDY (cpu *c, addr a) {
  c->Y = mem_read(c->mem, a);
  set_zn_flags(c, c->Y);
}

/* store accumulator */
void STA (cpu *c, addr a) {
  mem_write(c->mem, a, c->A);
}

/* store X register */
void STX (cpu *c, addr a) {
  mem_write(c->mem, a, c->X);
}

/* store Y register */
void STY (cpu *c, addr a) {
  mem_write(c->mem, a, c->Y);
}

/* 
 * ----- register transfers -----
 */

/* these use implied addressing, so no second parameter needed */

/* transfer accumulator to X */
void TAX (cpu *c) {
  c->X = c->A;
  set_zn_flags(c, c->X);
}

/* transfer accumulator to Y */
void TAY (cpu *c) {
  c->Y = c->A;
  set_zn_flags(c, c->Y);
}

/* transfer X to accumulator */
void TXA (cpu *c) {
  c->A = c->X;
  set_zn_flags(c, c->A);
}

/* transfer Y to accumulator */
void TYA (cpu *c) {
  c->A = c->Y;
  set_zn_flags(c, c->A);
}

/* 
 * ----- stack operations -----
 */

/* these also use implied addressing, so no second parameter 
 * also, recall SP points to the first empty spot on the stack
 * and that it grows downward */

/* transfer stack pointer to X */
void TSX (cpu *c) {
  c->X = c->SP;
  set_zn_flags(c, c->X);
}

/* transfer X to stack pointer */
void TXS (cpu *c) {
  c->SP = c->X;
}

/* push accumulator onto the stack */
void PHA (cpu *c) {
  push_byte(c, c->A);
}

/* push processor status onto stack */
void PHP (cpu *c) {
  /* bit 4 is always set when pushed to stack */
  push_byte(c, c->P | 0x10);
}

/* pull accumulator from stack */
void PLA (cpu *c) {
  c->A = pull_byte(c);
  /* set zero and negative flags */
  set_zn_flags(c, c->A);
}

/* pull processor status from stack */
void PLP (cpu *c) {
  /* bit 4 is never set when pulled from stack */
  /* bit 5 is always set */
  c->P = (pull_byte(c) & ~(0x10)) | 0x20;
}

/* 
 * ----- logical operations -----
 */

/* logical and */
void AND (cpu *c, addr a) {         
  c->A &= mem_read(c->mem, a);
  set_zn_flags(c, c->A);
}

/* exclusive or */
void EOR (cpu *c, addr a) {         
  c->A ^= mem_read(c->mem, a);
  set_zn_flags(c, c->A);
}

/* inclusive or */
void ORA (cpu *c, addr a) {         
  c->A |= mem_read(c->mem, a);
  set_zn_flags(c, c->A);
}

/* bit test */
void BIT (cpu *c, addr a) {
  byte b = mem_read(c->mem, a);
  /* set zero flag according and but don't keep result */
  set_flag(c, Z, !(b & c->A));
  /* set overflow and negative flags to bits 6,7 */
  set_flag(c, V, b & 0x40);
  set_flag(c, N, b & 0x80);
}

/* 
 * ----- arithmetic operations -----
 */

/* add with carry */
void ADC (cpu *c, addr a) {
  byte sum, j, k, c6, c7;
  byte b = mem_read(c->mem, a);
  /* NES actually ignores decimal mode, but you could enter this if in 
   * decimal mode and it should work. */
  if (0) {
    /* in decimal mode, j and k will be the decimal interpreted operands */
    j = 10 * (b >> 4) + (b & 0x0F);
    k = 10 * (c->A >> 4) + (c->A & 0x0F);
    sum = j + k + get_flag(c, C);
    /* set flag if sum exceeds 99 */
    set_flag(c, C, sum > 99);
    /* encode the result into accumulator */
    c->A = ((sum % 100 / 10) << 4) | (sum % 10);
    /* on a true 6502, the Z,N,V flags aren't valid in binary mode, but zero 
     * and negative can flags behave normally on others, even though negative 
     * flag's meaning is weird */
    set_zn_flags(c, c->A);
    /* V flag isnt valid, but maybe we'll deal with it in a later verison */

  } else {
    /* binary mode ADC can seem weird, but you really only need the leading 
     * bits of the operands and the 6 carry bit from the partial sum. refer to 
     * http://www.righto.com/2012/12/the-6502-overflow-flag-explained.html */
    /* j and k will represent the first bits of the operands */
    j = (b >> 7) & 0x1;
    k = (c->A >> 7) & 0x1;
    sum = c->A + b + get_flag(c, C);
    /* c6 is the 6th carry bit */
    c6 = j ^ k ^ ((sum >> 7) & 0x1);
    /* c7 is the outgoing carry bit */
    c7 = (j & k) | (j & c6) | (k & c6);
    /* set the appriopriate flags */
    set_flag(c, C, c7);
    set_flag(c, V, c6 ^ c7);
    set_flag(c, Z, !sum);
    set_flag(c, N, j ^ k ^ c6);
    c->A = sum;
  }
}

/* subtract with carry (borrow) */
void SBC (cpu *c, addr a) {
  /* just use the addition logic on the negated byte */
  mem_write(c->mem, a, ~mem_read(c->mem, a));
  ADC(c, a);
  mem_write(c->mem, a, ~mem_read(c->mem, a));
}

/* compare accumulator */
void CMP (cpu *c, addr a) {
  byte b, r;
  b = mem_read(c->mem, a);
  r = c->A - b;
  /* set flags based on difference */
  set_zn_flags(c, r);
  set_flag(c, C, c->A >= b);
}

/* compare X register */
void CPX (cpu *c, addr a) {
  byte b, r;
  b = mem_read(c->mem, a);
  r = c->X - b;
  /* set flags based on difference */
  set_zn_flags(c, r);
  set_flag(c, C, c->X >= b);
}

/* compare Y register */
void CPY (cpu *c, addr a) {
  byte b, r;
  b = mem_read(c->mem, a);
  r = c->Y - b;
  /* set flags based on difference */
  set_zn_flags(c, r);
  set_flag(c, C, c->Y >= b);
}

/* 
 * ----- increments & decrements -----
 */

/* inc/dec on registers use implied addressing, so no second parameter */

/* increment a memory location */
void INC (cpu *c, addr a) {
  byte b = mem_read(c->mem, a) + 1;
  mem_write(c->mem, a, b);
  set_zn_flags(c, b);
}

/* increment X register */
void INX (cpu *c) {
  c->X++;
  set_zn_flags(c, c->X);
}

/* increment Y register */
void INY (cpu *c) {
  c->Y++;
  set_zn_flags(c, c->Y);
}

/* decrement a memory location */
void DEC (cpu *c, addr a) {
  byte b = mem_read(c->mem, a) - 1;
  mem_write(c->mem, a, b);
  set_zn_flags(c, b);
}

/* decrement X register */
void DEX (cpu *c) {
  c->X--;
  set_zn_flags(c, c->X);
}

/* decrement Y register */
void DEY (cpu *c) {
  c->Y--;
  set_zn_flags(c, c->Y);
}

/* 
 * ----- shifts -----
 */

/* these operations can work directly on the accumulator, 
 * and they will be split off into separate functions */

/* arithmetic shift left */
void ASLa (cpu *c) {
  set_flag(c, C, c->A & 0x80);
  c->A <<= 1;
  set_zn_flags(c, c->A);
}

void ASL (cpu *c, addr a) {
  byte b = mem_read(c->mem, a);
  set_flag(c, C, b & 0x80); 
  b <<= 1;
  mem_write(c->mem, a, b);
  set_zn_flags(c, b);
}

/* logical shift right */
void LSRa (cpu *c) {
  set_flag(c, C, c->A & 0x1);
  c->A >>= 1;
  set_zn_flags(c, c->A);
} 

void LSR (cpu *c, addr a) {
  byte b = mem_read(c->mem, a);
  set_flag(c, C, b & 0x1); 
  b >>= 1;
  mem_write(c->mem, a, b);
  set_zn_flags(c, b);
}

/* rotate left */
void ROLa (cpu *c) {
  byte bit;
  bit = (c->A >> 7 & 0x1);
  c->A = (c->A << 1) | get_flag(c, C);
  set_flag(c, C, bit);
  set_zn_flags(c, c->A);
} 

void ROL (cpu *c, addr a) {
  byte bit, b;
  b = mem_read(c->mem, a);
  bit = (b >> 7) & 0x1;
  b = (b << 1) | get_flag(c, C);
  mem_write(c->mem, a, b);
  set_flag(c, C, bit);
  set_zn_flags(c, b);
}


/* rotate right */
void RORa (cpu *c) {
  byte bit;
  bit = c->A & 0x1;
  c->A = (c->A >> 1) | (get_flag(c, C) << 7);
  set_flag(c, C, bit);
  set_zn_flags(c, c->A);
} 

void ROR (cpu *c, addr a) { 
  byte bit, b;
  b = mem_read(c->mem, a);
  bit = b & 0x1;
  b = (b >> 1) | (get_flag(c, C) << 7);
  mem_write(c->mem, a, b);
  set_flag(c, C, bit);
  set_zn_flags(c, b);
}

/* 
 * ----- jumps & calls -----
 */

/* return uses implied addressing, so no second parameter */

/* jump to another location */
void JMP (cpu *c, addr a) {
  c->PC = a;
}

/* jump to subroutine */
void JSR (cpu *c, addr a) {
  push_word(c, c->PC - 1);
  c->PC = a;
}

/* return from subroutine */
void RTS (cpu *c) {
  c->PC = pull_word(c) + 1;
}

/* 
 * ----- branches -----
 */

/* branch if carry set */
void BCS (cpu *c, addr a) {
  if (get_flag(c, C))
    c->PC += (int8_t)mem_read(c->mem, a);
}

/* branch if carry clear */
void BCC (cpu *c, addr a) {
  if (!get_flag(c, C))
    c->PC += (int8_t)mem_read(c->mem, a);
}

/* branch if equal (zero set) */
void BEQ (cpu *c, addr a) {
  if (get_flag(c, Z))
    c->PC += (int8_t)mem_read(c->mem, a);
}

/* branch if not equal (zero clear) */
void BNE (cpu *c, addr a) {
  if (!get_flag(c, Z))
    c->PC += (int8_t)mem_read(c->mem, a);
}

/* branch if minus (negative set) */
void BMI (cpu *c, addr a) {
  if (get_flag(c, N))
    c->PC += (int8_t)mem_read(c->mem, a);
}

/* branch if positive (negative clear) */
void BPL (cpu *c, addr a) {
  if (!get_flag(c, N))
    c->PC += (int8_t)mem_read(c->mem, a);
}

/* branch if overflow set */
void BVS (cpu *c, addr a) {
  if (get_flag(c, V))
    c->PC += (int8_t)mem_read(c->mem, a);
}

/* branch if overflow clear */
void BVC (cpu *c, addr a) {
  if (!get_flag(c, V))
    c->PC += (int8_t)mem_read(c->mem, a);
}

/* 
 * ----- status flag changes -----
 */

/* these all use implied addressing */

/* clear carry flag */
void CLC (cpu *c) {
  set_flag(c, C, 0);
}

/* clear decimal flag */
void CLD (cpu *c) {
  set_flag(c, D, 0);
}

/* clear interrupt flag */
void CLI (cpu *c) {
  set_flag(c, I, 0);
}

/* clear overflow flag */
void CLV (cpu *c) {
  set_flag(c, V, 0);
}

/* set carry flag */
void SEC (cpu *c) {
  set_flag(c, C, 1);
}

/* set decimal flag */
void SED (cpu *c) {
  set_flag(c, D, 1);
}

/* set interrupt flag */
void SEI (cpu *c) {
  set_flag(c, I, 1);
}

/* 
 * ----- system functions -----
 */

/* these also use implied addressing */

/* force interrupt */
void BRK (cpu *c) {
  byte lo, hi;
  /* push PC then P onto the stack */
  /* bit 4 is always set when pushed to the stack */
  push_word(c, c->PC);
  push_byte(c, c->P | 0x10);
  /* set break flag */
  set_flag(c, B, 1);
  /* load interrupt vector at 0xFFFE/F */
  lo = mem_read(c->mem, 0xfffe);
  hi = mem_read(c->mem, 0xffff);
  c->PC = ((addr)(hi) << 8) | lo;
}

/* no operation */
void NOP (cpu *c) {
  /* silence compiler */
  (void)c;
}

/* skip byte (illegal) */
void SKB (cpu *c) {
  c->PC++;
}

/* skip word (illegal) */
void SKW (cpu *c) {
  c->PC += 2;
}

void RTI (cpu *c) {
  /* pull P then PC from the stack */
  c->P = (pull_byte(c) & ~(0x10)) | 0x20;
  c->PC = pull_word(c);
}


/* 
 * ---------- user functions ---------- 
 */

/* initializes a cpu */
void cpu_init (cpu *c, memory *mem) {
  /* link to the memory */
  c->mem = mem;
  /* only these flags are guaranteed at startup */
  set_flag(c, I, 1);
  set_flag(c, D, 0);
  /* this isn't a flag, but it's always 1 */
  set_flag(c, 5, 1);
  /* set stack pointer */
  c->SP = 0xfd;
  /* clear registers */
  c->A = 0;
  c->X = 0;
  c->Y = 0;
  /* load program counter to address at 0xfffc/d*/
  c->PC = ((addr)(mem_read(c->mem, 0xfffd)) << 8) | mem_read(c->mem, 0xfffc);
}

/* returns 0 on successful execution of a single instruction */
int cpu_step (cpu *c) {
  byte op;

  op = mem_read(c->mem, c->PC);

  printf("%04X  %02X A:%02X X:%02X Y:%02X P:%02X SP:%02X\n",
         c->PC, op, c->A, c->X, c->Y, c->P, c->SP);

  c->PC++;

  /* alphabetical by instruction */
  switch (op) {
    /* ADC */
  case 0x69: ADC(c, am_imm(c));        break;
  case 0x65: ADC(c, am_zer(c));        break;
  case 0x75: ADC(c, am_zex(c));      break;
  case 0x6d: ADC(c, am_abs(c));         break; 
  case 0x7d: ADC(c, am_abx(c));       break;
  case 0x79: ADC(c, am_aby(c));       break;
  case 0x61: ADC(c, am_inx(c)); break;
  case 0x71: ADC(c, am_iny(c)); break;
    /* AND */
  case 0x29: AND(c, am_imm(c));        break;
  case 0x25: AND(c, am_zer(c));        break;
  case 0x35: AND(c, am_zex(c));      break;
  case 0x2d: AND(c, am_abs(c));         break; 
  case 0x3d: AND(c, am_abx(c));       break;
  case 0x39: AND(c, am_aby(c));       break;
  case 0x21: AND(c, am_inx(c)); break;
  case 0x31: AND(c, am_iny(c)); break;
    /* ASL */
  case 0x0a: ASLa(c);      break;
  case 0x06: ASL(c, am_zer(c));        break;
  case 0x16: ASL(c, am_zex(c));      break;
  case 0x0e: ASL(c, am_abs(c));         break; 
  case 0x1e: ASL(c, am_abx(c));       break;
    /* BIT */
  case 0x24: BIT(c, am_zer(c));        break;
  case 0x2c: BIT(c, am_abs(c));         break;
    /* branches */
  case 0x10: BPL(c, am_rel(c));         break;
  case 0x30: BMI(c, am_rel(c));         break;
  case 0x50: BVC(c, am_rel(c));         break;
  case 0x70: BVS(c, am_rel(c));         break;
  case 0x90: BCC(c, am_rel(c));         break;
  case 0xb0: BCS(c, am_rel(c));         break;
  case 0xd0: BNE(c, am_rel(c));         break;
  case 0xf0: BEQ(c, am_rel(c));         break;
    /* BRK */
  case 0x00: BRK(c); /* implied operand */     break;
    /* CMP */
  case 0xc9: CMP(c, am_imm(c));        break;
  case 0xc5: CMP(c, am_zer(c));        break;
  case 0xd5: CMP(c, am_zex(c));      break;
  case 0xcd: CMP(c, am_abs(c));         break; 
  case 0xdd: CMP(c, am_abx(c));       break;
  case 0xd9: CMP(c, am_aby(c));       break;
  case 0xc1: CMP(c, am_inx(c)); break;
  case 0xd1: CMP(c, am_iny(c)); break;
    /* CPX */
  case 0xe0: CPX(c, am_imm(c));        break;
  case 0xe4: CPX(c, am_zer(c));        break;
  case 0xec: CPX(c, am_abs(c));         break;
    /* CPY */
  case 0xc0: CPY(c, am_imm(c));        break;
  case 0xc4: CPY(c, am_zer(c));        break;
  case 0xcc: CPY(c, am_abs(c));         break;
    /* DEC */
  case 0xc6: DEC(c, am_zer(c));        break;
  case 0xd6: DEC(c, am_zex(c));      break;
  case 0xce: DEC(c, am_abs(c));         break;
  case 0xde: DEC(c, am_abx(c));       break;
    /* EOR */
  case 0x49: EOR(c, am_imm(c));        break;
  case 0x45: EOR(c, am_zer(c));        break;
  case 0x55: EOR(c, am_zex(c));      break;
  case 0x4d: EOR(c, am_abs(c));         break; 
  case 0x5d: EOR(c, am_abx(c));       break;
  case 0x59: EOR(c, am_aby(c));       break;
  case 0x41: EOR(c, am_inx(c)); break;
  case 0x51: EOR(c, am_iny(c)); break;
    /* status flag operations */
  case 0x18: CLC(c); /* implied operand */     break;
  case 0x38: SEC(c); /* implied operand */     break;
  case 0x58: CLI(c); /* implied operand */     break;
  case 0x78: SEI(c); /* implied operand */     break;
  case 0xb8: CLV(c); /* implied operand */     break;
  case 0xd8: CLD(c); /* implied operand */     break;
  case 0xf8: SED(c); /* implied operand */     break;
    /* INC */
  case 0xe6: INC(c, am_zer(c));        break;
  case 0xf6: INC(c, am_zex(c));      break;
  case 0xee: INC(c, am_abs(c));         break;
  case 0xfe: INC(c, am_abx(c));       break;
    /* JMP */
  case 0x4c: JMP(c, am_abs(c));         break;
  case 0x6c: JMP(c, am_ind(c));         break;
    /* JSR */
  case 0x20: JSR(c, am_abs(c));         break;
    /* LDA */
  case 0xa9: LDA(c, am_imm(c));        break;
  case 0xa5: LDA(c, am_zer(c));        break;
  case 0xb5: LDA(c, am_zex(c));      break;
  case 0xad: LDA(c, am_abs(c));         break; 
  case 0xbd: LDA(c, am_abx(c));       break;
  case 0xb9: LDA(c, am_aby(c));       break;
  case 0xa1: LDA(c, am_inx(c)); break;
  case 0xb1: LDA(c, am_iny(c)); break;
    /* LDX */
  case 0xa2: LDX(c, am_imm(c));        break;
  case 0xa6: LDX(c, am_zer(c));        break;
  case 0xb6: LDX(c, am_zey(c));      break;
  case 0xae: LDX(c, am_abs(c));         break;
  case 0xbe: LDX(c, am_aby(c));       break;
    /* LDY */
  case 0xa0: LDY(c, am_imm(c));        break;
  case 0xa4: LDY(c, am_zer(c));        break;
  case 0xb4: LDY(c, am_zex(c));      break;
  case 0xac: LDY(c, am_abs(c));         break;
  case 0xbc: LDY(c, am_abx(c));       break;
    /* LSR */
  case 0x4a: LSRa(c);      break;
  case 0x46: LSR(c, am_zer(c));        break;
  case 0x56: LSR(c, am_zex(c));      break;
  case 0x4e: LSR(c, am_abs(c));         break;
  case 0x5e: LSR(c, am_abx(c));       break;
    /* NOP and it's extra illegal codes */
  case 0x1a:
  case 0x3a:
  case 0x5a:
  case 0x7a:
  case 0xda:
  case 0xfa:
  case 0xea: NOP(c); /* implied operand */     break;
    /* ORA */
  case 0x09: ORA(c, am_imm(c));        break;
  case 0x05: ORA(c, am_zer(c));        break;
  case 0x15: ORA(c, am_zex(c));      break;
  case 0x0d: ORA(c, am_abs(c));         break; 
  case 0x1d: ORA(c, am_abx(c));       break;
  case 0x19: ORA(c, am_aby(c));       break;
  case 0x01: ORA(c, am_inx(c)); break;
  case 0x11: ORA(c, am_iny(c)); break;
    /* register instructions */
  case 0xaa: TAX(c); /* implied operand */     break;
  case 0x8a: TXA(c); /* implied operand */     break;
  case 0xca: DEX(c); /* implied operand */     break;
  case 0xe8: INX(c); /* implied operand */     break;
  case 0xa8: TAY(c); /* implied operand */     break;
  case 0x98: TYA(c); /* implied operand */     break;
  case 0x88: DEY(c); /* implied operand */     break;
  case 0xc8: INY(c); /* implied operand */     break;
    /* ROL */
  case 0x2a: ROLa(c);      break;
  case 0x26: ROL(c, am_zer(c));        break;
  case 0x36: ROL(c, am_zex(c));      break;
  case 0x2e: ROL(c, am_abs(c));         break;
  case 0x3e: ROL(c, am_abx(c));       break;
    /* ROR */
  case 0x6a: RORa(c);      break;
  case 0x66: ROR(c, am_zer(c));        break;
  case 0x76: ROR(c, am_zex(c));      break;
  case 0x6e: ROR(c, am_abs(c));         break;
  case 0x7e: ROR(c, am_abx(c));       break;
    /* RTI */
  case 0x40: RTI(c); /* implied operand */     break;
    /* RTS */
  case 0x60: RTS(c); /* implied operand */     break;
    /* SBC */
  case 0xe9: SBC(c, am_imm(c));        break;
  case 0xe5: SBC(c, am_zer(c));        break;
  case 0xf5: SBC(c, am_zex(c));      break;
  case 0xed: SBC(c, am_abs(c));         break; 
  case 0xfd: SBC(c, am_abx(c));       break;
  case 0xf9: SBC(c, am_aby(c));       break;
  case 0xe1: SBC(c, am_inx(c)); break;
  case 0xf1: SBC(c, am_iny(c)); break;
    /* STA */
  case 0x85: STA(c, am_zer(c));        break;
  case 0x95: STA(c, am_zex(c));      break;
  case 0x8d: STA(c, am_abs(c));         break; 
  case 0x9d: STA(c, am_abx(c));       break;
  case 0x99: STA(c, am_aby(c));       break;
  case 0x81: STA(c, am_inx(c)); break;
  case 0x91: STA(c, am_iny(c)); break;
    /* stack instructions */
  case 0x9a: TXS(c); /* implied operand */     break;
  case 0xba: TSX(c); /* implied operand */     break;
  case 0x48: PHA(c); /* implied operand */     break;
  case 0x68: PLA(c); /* implied operand */     break;
  case 0x08: PHP(c); /* implied operand */     break;
  case 0x28: PLP(c); /* implied operand */     break;
    /* STX */
  case 0x86: STX(c, am_zer(c));        break;
  case 0x96: STX(c, am_zey(c));      break;
  case 0x8e: STX(c, am_abs(c));         break;
    /* STY */
  case 0x84: STY(c, am_zer(c));        break;
  case 0x94: STY(c, am_zex(c));      break;
  case 0x8c: STY(c, am_abs(c));         break;
    /* illegal opcodes go to SKB skip byte / SKW skip word */
  case 0x80:
  case 0x82:
  case 0xc2:
  case 0xe2:
  case 0x04:
  case 0x14:
  case 0x34:
  case 0x44:
  case 0x54:
  case 0x64:
  case 0x74:
  case 0xd4:
  case 0xf4: SKB(c); /* implied operand */     break;
  case 0x0c:
  case 0x1c:
  case 0x3c:
  case 0x5c:
  case 0x7c:
  case 0xdc:
  case 0xfc: SKW(c); /* implied operand */     break;

    /* error */
  default: 
    /* invalid op, returning it so we can solve it or crash */
    printf("Invalid opcode: 0x%02x\n", op);
    return 1;
  }

  /* return 0 to indicate success, 0 would never be returned in success
   * because we know 0x00 is a valid operation (BRK) */
  return 0;
}
