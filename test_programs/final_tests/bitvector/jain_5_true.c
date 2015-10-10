extern int __VERIFIER_nondet_int(void);
void main()
{
  int x,y;

  x=0;
  y=4;


  while(1)
    {
      x = x + y;
      y = y +4;
      
      
      __VERIFIER_assert(x!=30);
    }
}

