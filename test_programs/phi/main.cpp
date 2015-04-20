int __VERIFIER_nondet_int();

void compare(bool r, bool y) {
      bool l = y || r;
  }

int main() {
	int a =  __VERIFIER_nondet_int();
	int b =  __VERIFIER_nondet_int();
	int c = 10;
	if( a + b > 8) {
		c = 12;
	}
	else {
		c = 11;
	}
	return c;
}