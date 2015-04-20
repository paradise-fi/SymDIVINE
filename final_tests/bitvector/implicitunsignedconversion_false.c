int main() {
  unsigned int plus_one = 1;
  int minus_one = -1;

  if(plus_one < minus_one) {
    goto ERROR;
  }
  
  return (0);
  ERROR:assert( 0 );
  return (-1);
}

