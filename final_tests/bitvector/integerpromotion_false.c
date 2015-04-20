#include <stdio.h>
#include <stdint.h>

int main() {

  unsigned char port = 0x5a;
  unsigned char result_8 = ( ~port ) >> 4;
  if (result_8 == 0xfa) {
    printf ("UNSAFE\n");
    goto ERROR;
  }

  
  printf ("SAFE\n");
  
  return (0);
  ERROR:assert( 0 );
  return (-1);
}

