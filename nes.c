#include <stdio.h>
#include <sys/stat.h>
#include "6502.h"

int main (int argc, char** argv) {
  FILE *in;
  struct stat in_stat;
  cpu nes_cpu;

  if (argc != 2) {
    printf("Invalid number of arguments. Please supply just one ROM file.\n");
    return 1;
  }

  in = fopen(argv[1], "rb");    /* read only binary */
  if (!in) {
    printf("Given file '%s' could not be found.\n", argv[1]);
    return 1;
  }

  cpu_init(&nes_cpu);

  /* TODO: size checking */
  /* fstat(fileno(in), &in_stat); */

  /* if (in_stat.st_size < 0x4000)    /\* less than 16kB *\/ */
  /*   fread(nes_cpu.cartidge_upper_bank, sizeof(byte), in_stat.st_size, in); */
  /* else if (in_stat.st_size < 0x8000)    /\* less than 32kB *\/ */
  /*   fread(nes_cpu.cartidge_lower_bank, sizeof(byte), in_stat.st_size, in); */

  /* for the nestest rom */
  fread(nes_cpu.cartidge_upper_bank-16, sizeof(byte), 0x4000, in);
  nes_cpu.PC = 0xc000;

  for(int i = 0; i < 10000; i++) {
    cpu_step(&nes_cpu);
  }
 
  return 0;
}


















