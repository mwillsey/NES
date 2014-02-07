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
  c->mem[0x100 + c->SP] = b;
  c->SP--;
}

byte pull_byte (cpu *c) {
  c->SP++;
  return c->mem[0x100 + c->SP];
}

void push_word (cpu *c, word w) {
  /* push lo then hi*/
  push_byte(c, (byte)(w >> 8));
  push_byte(c, (byte)(w));
}

word pull_word (cpu *c) {
  byte lo, hi;
  /* pull hi then lo */
  lo = pull_byte(c);
  hi = pull_byte(c);
  return ((word)(hi) << 8) | lo;
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


/* addressing will be handled by handing every intruction a pointer
 * to the byte it will be operating on. That way intructions can be
 * coded without knowledge of their addressing mode. Addressing will also
 * handle incrementing the PC according to the number of arguments */

/* accumulator
 * the instruction will operate directly on the accumulator 
 */
byte *addr_accumulator (cpu *c) { 
  return &c->A;
}

/* immediate
 * the constant immediately follows in instruction in memory 
 * we will work with the invariant that the PC is incremented until 
 * the instruction is complete
 */
byte *addr_immediate (cpu *c) { 
  return &c->mem[c->PC++]; 
}

/* zero page 
 * only an 8-bit address operand is given, so only the first 0x100 (256) 
 * bytes of memory can be addressed (0x0000 - 0x00FF)
 */
byte *addr_zero_page (cpu *c) { 
  byte a;
  a = c->mem[c->PC++];
  //printf("%04x    ", (word)a);
  return &c->mem[(word)(a)];
}

/* zero page,x
 * address to be accessed is the 8-bit value given in the instruction 
 * summed with the X register
 */
byte *addr_zero_page_x (cpu *c) {
  byte a;
  a = c->mem[c->PC++] + c->X;
  return &c->mem[(word)(a)];
}

/* zero page,y
 * address to be accessed is the 8-bit value given in the instruction 
 * summed with the X register
 */
byte *addr_zero_page_y (cpu *c) {
  byte a;
  a = c->mem[c->PC++] + c->Y;
  return &c->mem[(word)(a)];
}

/* relative
 * addressing is identical to immediate, just handled in a signed manner
 * by the branch instructions
 */
byte *addr_relative (cpu *c) { 
  return &c->mem[c->PC++]; 
}

/* absolute
 * full address to be operated on is given 2 bytes following instruction 
 * don't forget the 6502 is least significant byte first
 */
byte *addr_absolute (cpu *c) {
  byte lo, hi;
  word a;
  lo = c->mem[c->PC++];
  hi = c->mem[c->PC++];
  a = ((word)(hi) << 8) | lo;
  return &c->mem[a];
}

/* absolute,x
 * uses the full address given in the 2 bytes following instruction, but
 * also adds the X register
 */
byte *addr_absolute_x (cpu *c) {
  byte lo, hi;
  word a;
  lo = c->mem[c->PC++];
  hi = c->mem[c->PC++];
  a = (((word)(hi) << 8) | lo)  + c->X;
  return &c->mem[a];
}

/* absolute,y
 * uses the full address given in the 2 bytes following instruction, but
 * also adds the Y register
 */
byte *addr_absolute_y (cpu *c) {
  byte lo, hi;
  word a;
  lo = c->mem[c->PC++];
  hi = c->mem[c->PC++];
  a = (((word)(hi) << 8) | lo) + (word)c->Y;
  return &c->mem[a];
}

/* indirect
 * only JMP uses indirect addressing, where the 2 byte following the 
 * instruction yield and address containing the lowest byte of the
 * address you want. For our purposes, this is identical to absolute 
 * addressing
 */
byte *addr_indirect (cpu *c) {
  byte lo, hi, lo2, hi2;
  word a, a2, a1;
  lo = c->mem[c->PC++];
  hi = c->mem[c->PC++];
  a = ((word)(hi) << 8) | lo;
  /* a1 is a + 1 in most cases, except to represnt a bug in the hardware 
   * where it really only adds 1 to the lo byte*/
  a1 = ((word)(hi) << 8) | (byte)(lo + 1);
  /* now start getting the target address */
  lo2 = c->mem[a];
  hi2 = c->mem[a1];
  a2 = ((word)(hi2) << 8) | lo2;
  return &c->mem[a2];
} 

/* indexed indirect
 * easy if you imaging an address table in zero page memory. The byte
 * following the instruction gives the base address, and the index is in 
 * the X register. That will contain the LSB of the target address
 */
byte *addr_indexed_indirect (cpu *c) {
  byte base, lo, hi;
  word a;
  base = c->mem[c->PC++];
  lo = base + c->X;
  hi = base + c->X + 1;
  a = ((word)(c->mem[hi]) << 8) | c->mem[lo];
  return &c->mem[a];
}

/* indirect indexed
 * byte following instruction is the zero page address of the LSB of a 16-bit
 * address. Then add the Y-register to that
 */
byte *addr_indirect_indexed (cpu *c) {
  byte z, lo, hi;
  word a;
  z = c->mem[c->PC++];
  lo = z;
  hi = z + 1;
  a = (((word)(c->mem[hi]) << 8) | c->mem[lo]) + c->Y;
  return &c->mem[a];
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
void LDA (cpu *c, byte *b) {
  c->A = *b;
  set_zn_flags(c, c->A);
}

/* load X register */
void LDX (cpu *c, byte *b) {
  c->X = *b;
  set_zn_flags(c, c->X);
}

/* load Y register */
void LDY (cpu *c, byte *b) {
  c->Y = *b;
  set_zn_flags(c, c->Y);
}

/* store accumulator */
void STA (cpu *c, byte *b) {
  *b = c->A;
}

/* store X register */
void STX (cpu *c, byte *b) {
  *b = c->X;
}

/* store Y register */
void STY (cpu *c, byte *b) {
  *b = c->Y;
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
void AND (cpu *c, byte *b) {         
  c->A &= *b;
  set_zn_flags(c, c->A);
}

/* exclusive or */
void EOR (cpu *c, byte *b) {         
  c->A ^= *b;
  set_zn_flags(c, c->A);
}

/* inclusive or */
void ORA (cpu *c, byte *b) {         
  c->A |= *b;
  set_zn_flags(c, c->A);
}

/* bit test */
void BIT (cpu *c, byte *b) {
  /* set zero flag according and but don't keep result */
  set_flag(c, Z, !(*b & c->A));
  /* set overflow and negative flags to bits 6,7 */
  set_flag(c, V, *b & 0x40);
  set_flag(c, N, *b & 0x80);
}

/* 
 * ----- arithmetic operations -----
 */

/* add with carry */
void ADC (cpu *c, byte *b) {
  byte sum, j, k, c6, c7;

  /* NES actually ignore decimal mode, but you could enter this if in 
   * decimal mode and it should work. */
  if (0) {
    /* in decimal mode, j and k will be the decimal interpreted operands */
    j = 10 * (*b >> 4) + (*b & 0x0F);
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
     * bits of the operands and the 6 carry bit from the partial sum. refer to: 
     * http://www.righto.com/2012/12/the-6502-overflow-flag-explained.html */
    /* j and k will represent the first bits of the operands */
    j = (*b >> 7) & 0x1;
    k = (c->A >> 7) & 0x1;
    sum = c->A + *b + get_flag(c, C);
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
void SBC (cpu *c, byte *b) {
  /* just use the addition logic on the negated byte */
  byte n = ~*b;
  ADC(c, &n);
}

/* compare accumulator */
void CMP (cpu *c, byte *b) {
  byte r;
  r = c->A - *b;
  /* set flags based on difference */
  set_zn_flags(c, r);
  set_flag(c, C, c->A >= *b);
}

/* compare X register */
void CPX (cpu *c, byte *b) {
  byte r;
  r = c->X - *b;
  /* set flags based on difference */
  set_zn_flags(c, r);
  set_flag(c, C, c->X >= *b);
}

/* compare Y register */
void CPY (cpu *c, byte *b) {
  byte r;
  r = c->Y - *b;
  /* set flags based on difference */
  set_zn_flags(c, r);
  set_flag(c, C, c->Y >= *b);
}

/* 
 * ----- increments & decrements -----
 */

/* inc/dec on registers use implied addressing, so no second parameter */

/* increment a memory location */
void INC (cpu *c, byte *b) {
  (*b)++;
  set_zn_flags(c, *b);
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
void DEC (cpu *c, byte *b) {
  (*b)--;
  set_zn_flags(c, *b);
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

/* arithmetic shift left */
void ASL (cpu *c, byte *b) {
  /* set carry if we're shifting a bit out */
  set_flag(c, C, *b & 0x80); 
  *b <<= 1;
  set_zn_flags(c, *b);
}

/* logical shift right */
void LSR (cpu *c, byte *b) {
  /* set carry if we're shifting a bit out */
  set_flag(c, C, *b & 0x1);
  *b >>= 1;
  set_zn_flags(c, *b);
}

/* rotate left */
void ROL (cpu *c, byte *b) {
  byte bit;
  /* save most significant bit to put in carry */
  bit = (*b >> 7) & 0x1;
  *b = (*b << 1) | get_flag(c, C);
  set_flag(c, C, bit);
  /* set zero and negative flags */
  set_zn_flags(c, *b);
}

/* rotate right */
void ROR (cpu *c, byte *b) {
  byte bit;
  /* save least significant bit to put in carry */
  bit = *b & 0x1;
  *b = (*b >> 1) | (get_flag(c, C) << 7);
  set_flag(c, C, bit);
  /* set zero and negative flags */
  set_zn_flags(c, *b);
}

/* 
 * ----- jumps & calls -----
 */

/* return uses implied addressing, so no second parameter */

/* jump to another location */
void JMP (cpu *c, byte *b) {
  /* jump to a word indicated by the operand */
  /* have to subtract because PC is an index into memory */
  c->PC = b - c->mem;
}

/* jump to subroutine */
void JSR (cpu *c, byte *b) {
  /* push PC to stack  */
  push_word(c, c->PC - 1);
  /* jump to the indicated address */
  /* have to subtract because PC is an index into memory */
  c->PC = b - c->mem;
}

/* return from subroutine */
void RTS (cpu *c) {
  /* pull PC from stack */
  c->PC = pull_word(c) + 1;
}

/* 
 * ----- branches -----
 */

/* branch if carry set */
void BCS (cpu *c, byte *b) {
  if (get_flag(c, C))
    c->PC += (int8_t)(*b);
}

/* branch if carry clear */
void BCC (cpu *c, byte *b) {
  if (!get_flag(c, C))
    c->PC += (int8_t)(*b);
}

/* branch if equal (zero set) */
void BEQ (cpu *c, byte *b) {
  if (get_flag(c, Z))
    c->PC += (int8_t)(*b);
}

/* branch if not equal (zero clear) */
void BNE (cpu *c, byte *b) {
  if (!get_flag(c, Z))
    c->PC += (int8_t)(*b);
}

/* branch if minus (negative set) */
void BMI (cpu *c, byte *b) {
  if (get_flag(c, N))
    c->PC += (int8_t)(*b);
}

/* branch if positive (negative clear) */
void BPL (cpu *c, byte *b) {
  if (!get_flag(c, N))
    c->PC += (int8_t)(*b);
}

/* branch if overflow set */
void BVS (cpu *c, byte *b) {
  if (get_flag(c, V))
    c->PC += (int8_t)(*b);
}

/* branch if overflow clear */
void BVC (cpu *c, byte *b) {
  if (!get_flag(c, V))
    c->PC += (int8_t)(*b);
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
  word a;
  /* push PC then P onto the stack */
  /* bit 4 is always set when pushed to the stack */
  push_word(c, c->PC);
  push_byte(c, c->P | 0x10);
  /* set break flag */
  set_flag(c, B, 1);
  /* load interrupt vector at 0xFFFE/F */
  lo = c->mem[0xFFFE];
  hi = c->mem[0xFFFF];
  a = ((word)(hi) << 8) | lo;
  c->PC = c->mem[a];
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
void cpu_init (cpu *c) {
  /* only these flags are guaranteed at startup */
  set_flag(c, I, 1);
  set_flag(c, D, 0);
  /* this isn't a flag, but it's always 1 */
  set_flag(c, 5, 1);
  /* set stack pointer */
  c->SP = 0xfd;
  /* load program counter to address at 0xfffc/d*/
  c->PC = ((word)(c->mem[0xfffd]) << 4) | c->mem[0xfffc];
  /* initialize pointers to memory */
  c->cartidge_upper_bank = &c->mem[0xc000];
  c->cartidge_lower_bank = &c->mem[0x8000];
}

/* returns 0 on successful execution of a single instruction */
int cpu_step (cpu *c) {
  byte op;

  op = c->mem[c->PC];

  printf("%04X  %02X A:%02X X:%02X Y:%02X P:%02X SP:%02X\n",
         c->PC, op, c->A, c->X, c->Y, c->P, c->SP);

  c->PC++;

  /* alphabetical by instruction */
  switch (op) {
    /* ADC */
  case 0x69: ADC(c, addr_immediate(c));        break;
  case 0x65: ADC(c, addr_zero_page(c));        break;
  case 0x75: ADC(c, addr_zero_page_x(c));      break;
  case 0x6d: ADC(c, addr_absolute(c));         break; 
  case 0x7d: ADC(c, addr_absolute_x(c));       break;
  case 0x79: ADC(c, addr_absolute_y(c));       break;
  case 0x61: ADC(c, addr_indexed_indirect(c)); break;
  case 0x71: ADC(c, addr_indirect_indexed(c)); break;
    /* AND */
  case 0x29: AND(c, addr_immediate(c));        break;
  case 0x25: AND(c, addr_zero_page(c));        break;
  case 0x35: AND(c, addr_zero_page_x(c));      break;
  case 0x2d: AND(c, addr_absolute(c));         break; 
  case 0x3d: AND(c, addr_absolute_x(c));       break;
  case 0x39: AND(c, addr_absolute_y(c));       break;
  case 0x21: AND(c, addr_indexed_indirect(c)); break;
  case 0x31: AND(c, addr_indirect_indexed(c)); break;
    /* ASL */
  case 0x0a: ASL(c, addr_accumulator(c));      break;
  case 0x06: ASL(c, addr_zero_page(c));        break;
  case 0x16: ASL(c, addr_zero_page_x(c));      break;
  case 0x0e: ASL(c, addr_absolute(c));         break; 
  case 0x1e: ASL(c, addr_absolute_x(c));       break;
    /* BIT */
  case 0x24: BIT(c, addr_zero_page(c));        break;
  case 0x2c: BIT(c, addr_absolute(c));         break;
    /* branches */
  case 0x10: BPL(c, addr_relative(c));         break;
  case 0x30: BMI(c, addr_relative(c));         break;
  case 0x50: BVC(c, addr_relative(c));         break;
  case 0x70: BVS(c, addr_relative(c));         break;
  case 0x90: BCC(c, addr_relative(c));         break;
  case 0xb0: BCS(c, addr_relative(c));         break;
  case 0xd0: BNE(c, addr_relative(c));         break;
  case 0xf0: BEQ(c, addr_relative(c));         break;
    /* BRK */
  case 0x00: BRK(c); /* implied operand */     break;
    /* CMP */
  case 0xc9: CMP(c, addr_immediate(c));        break;
  case 0xc5: CMP(c, addr_zero_page(c));        break;
  case 0xd5: CMP(c, addr_zero_page_x(c));      break;
  case 0xcd: CMP(c, addr_absolute(c));         break; 
  case 0xdd: CMP(c, addr_absolute_x(c));       break;
  case 0xd9: CMP(c, addr_absolute_y(c));       break;
  case 0xc1: CMP(c, addr_indexed_indirect(c)); break;
  case 0xd1: CMP(c, addr_indirect_indexed(c)); break;
    /* CPX */
  case 0xe0: CPX(c, addr_immediate(c));        break;
  case 0xe4: CPX(c, addr_zero_page(c));        break;
  case 0xec: CPX(c, addr_absolute(c));         break;
    /* CPY */
  case 0xc0: CPY(c, addr_immediate(c));        break;
  case 0xc4: CPY(c, addr_zero_page(c));        break;
  case 0xcc: CPY(c, addr_absolute(c));         break;
    /* DEC */
  case 0xc6: DEC(c, addr_zero_page(c));        break;
  case 0xd6: DEC(c, addr_zero_page_x(c));      break;
  case 0xce: DEC(c, addr_absolute(c));         break;
  case 0xde: DEC(c, addr_absolute_x(c));       break;
    /* EOR */
  case 0x49: EOR(c, addr_immediate(c));        break;
  case 0x45: EOR(c, addr_zero_page(c));        break;
  case 0x55: EOR(c, addr_zero_page_x(c));      break;
  case 0x4d: EOR(c, addr_absolute(c));         break; 
  case 0x5d: EOR(c, addr_absolute_x(c));       break;
  case 0x59: EOR(c, addr_absolute_y(c));       break;
  case 0x41: EOR(c, addr_indexed_indirect(c)); break;
  case 0x51: EOR(c, addr_indirect_indexed(c)); break;
    /* status flag operations */
  case 0x18: CLC(c); /* implied operand */     break;
  case 0x38: SEC(c); /* implied operand */     break;
  case 0x58: CLI(c); /* implied operand */     break;
  case 0x78: SEI(c); /* implied operand */     break;
  case 0xb8: CLV(c); /* implied operand */     break;
  case 0xd8: CLD(c); /* implied operand */     break;
  case 0xf8: SED(c); /* implied operand */     break;
    /* INC */
  case 0xe6: INC(c, addr_zero_page(c));        break;
  case 0xf6: INC(c, addr_zero_page_x(c));      break;
  case 0xee: INC(c, addr_absolute(c));         break;
  case 0xfe: INC(c, addr_absolute_x(c));       break;
    /* JMP */
  case 0x4c: JMP(c, addr_absolute(c));         break;
  case 0x6c: JMP(c, addr_indirect(c));         break;
    /* JSR */
  case 0x20: JSR(c, addr_absolute(c));         break;
    /* LDA */
  case 0xa9: LDA(c, addr_immediate(c));        break;
  case 0xa5: LDA(c, addr_zero_page(c));        break;
  case 0xb5: LDA(c, addr_zero_page_x(c));      break;
  case 0xad: LDA(c, addr_absolute(c));         break; 
  case 0xbd: LDA(c, addr_absolute_x(c));       break;
  case 0xb9: LDA(c, addr_absolute_y(c));       break;
  case 0xa1: LDA(c, addr_indexed_indirect(c)); break;
  case 0xb1: LDA(c, addr_indirect_indexed(c)); break;
    /* LDX */
  case 0xa2: LDX(c, addr_immediate(c));        break;
  case 0xa6: LDX(c, addr_zero_page(c));        break;
  case 0xb6: LDX(c, addr_zero_page_y(c));      break;
  case 0xae: LDX(c, addr_absolute(c));         break;
  case 0xbe: LDX(c, addr_absolute_y(c));       break;
    /* LDY */
  case 0xa0: LDY(c, addr_immediate(c));        break;
  case 0xa4: LDY(c, addr_zero_page(c));        break;
  case 0xb4: LDY(c, addr_zero_page_x(c));      break;
  case 0xac: LDY(c, addr_absolute(c));         break;
  case 0xbc: LDY(c, addr_absolute_x(c));       break;
    /* LSR */
  case 0x4a: LSR(c, addr_accumulator(c));      break;
  case 0x46: LSR(c, addr_zero_page(c));        break;
  case 0x56: LSR(c, addr_zero_page_x(c));      break;
  case 0x4e: LSR(c, addr_absolute(c));         break;
  case 0x5e: LSR(c, addr_absolute_x(c));       break;
    /* NOP and it's extra illegal codes */
  case 0x1a:
  case 0x3a:
  case 0x5a:
  case 0x7a:
  case 0xda:
  case 0xfa:
  case 0xea: NOP(c); /* implied operand */     break;
    /* ORA */
  case 0x09: ORA(c, addr_immediate(c));        break;
  case 0x05: ORA(c, addr_zero_page(c));        break;
  case 0x15: ORA(c, addr_zero_page_x(c));      break;
  case 0x0d: ORA(c, addr_absolute(c));         break; 
  case 0x1d: ORA(c, addr_absolute_x(c));       break;
  case 0x19: ORA(c, addr_absolute_y(c));       break;
  case 0x01: ORA(c, addr_indexed_indirect(c)); break;
  case 0x11: ORA(c, addr_indirect_indexed(c)); break;
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
  case 0x2a: ROL(c, addr_accumulator(c));      break;
  case 0x26: ROL(c, addr_zero_page(c));        break;
  case 0x36: ROL(c, addr_zero_page_x(c));      break;
  case 0x2e: ROL(c, addr_absolute(c));         break;
  case 0x3e: ROL(c, addr_absolute_x(c));       break;
    /* ROR */
  case 0x6a: ROR(c, addr_accumulator(c));      break;
  case 0x66: ROR(c, addr_zero_page(c));        break;
  case 0x76: ROR(c, addr_zero_page_x(c));      break;
  case 0x6e: ROR(c, addr_absolute(c));         break;
  case 0x7e: ROR(c, addr_absolute_x(c));       break;
    /* RTI */
  case 0x40: RTI(c); /* implied operand */     break;
    /* RTS */
  case 0x60: RTS(c); /* implied operand */     break;
    /* SBC */
  case 0xe9: SBC(c, addr_immediate(c));        break;
  case 0xe5: SBC(c, addr_zero_page(c));        break;
  case 0xf5: SBC(c, addr_zero_page_x(c));      break;
  case 0xed: SBC(c, addr_absolute(c));         break; 
  case 0xfd: SBC(c, addr_absolute_x(c));       break;
  case 0xf9: SBC(c, addr_absolute_y(c));       break;
  case 0xe1: SBC(c, addr_indexed_indirect(c)); break;
  case 0xf1: SBC(c, addr_indirect_indexed(c)); break;
    /* STA */
  case 0x85: STA(c, addr_zero_page(c));        break;
  case 0x95: STA(c, addr_zero_page_x(c));      break;
  case 0x8d: STA(c, addr_absolute(c));         break; 
  case 0x9d: STA(c, addr_absolute_x(c));       break;
  case 0x99: STA(c, addr_absolute_y(c));       break;
  case 0x81: STA(c, addr_indexed_indirect(c)); break;
  case 0x91: STA(c, addr_indirect_indexed(c)); break;
    /* stack instructions */
  case 0x9a: TXS(c); /* implied operand */     break;
  case 0xba: TSX(c); /* implied operand */     break;
  case 0x48: PHA(c); /* implied operand */     break;
  case 0x68: PLA(c); /* implied operand */     break;
  case 0x08: PHP(c); /* implied operand */     break;
  case 0x28: PLP(c); /* implied operand */     break;
    /* STX */
  case 0x86: STX(c, addr_zero_page(c));        break;
  case 0x96: STX(c, addr_zero_page_y(c));      break;
  case 0x8e: STX(c, addr_absolute(c));         break;
    /* STY */
  case 0x84: STY(c, addr_zero_page(c));        break;
  case 0x94: STY(c, addr_zero_page_x(c));      break;
  case 0x8c: STY(c, addr_absolute(c));         break;
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
