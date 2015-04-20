#include <assert.h>

int __VERIFIER_nondet_int();

int main() {
	while(true) {
		int input = __VERIFIER_nondet_int();
		unsigned short res = (unsigned short) input + 2;
		assert(res == input + 2);
	}
}