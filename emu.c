#include "nes.h"

int main (int argc, char** argv) {
  FILE *in;
  //struct stat in_stat;
  nes n;

  if (argc != 2) {
    printf("Invalid number of arguments. Please supply just one ROM file.\n");
    return 1;
  }

  in = fopen(argv[1], "rb");    /* read only binary */
  if (!in) {
    printf("Given file '%s' could not be found.\n", argv[1]);
    return 1;
  }

  nes_init(&n);

  
  /* TODO: size checking */
  /* fstat(fileno(in), &in_stat); */

  /* if (in_stat.st_size < 0x4000)    /\* less than 16kB *\/ */
  /*   fread(nes_cpu.cartidge_upper_bank, sizeof(byte), in_stat.st_size, in); */
  /* else if (in_stat.st_size < 0x8000)    /\* less than 32kB *\/ */
  /*   fread(nes_cpu.cartidge_lower_bank, sizeof(byte), in_stat.st_size, in); */

  /* for the nestest rom */
  fread(n.upper_bank-16, sizeof(byte), 0x4000, in);

  int i;
  for(i = 0; i < 10000; i++) {
    nes_step(&n);
  }
 
  return 0;
}
