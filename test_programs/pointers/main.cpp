#include <assert.h>
int __VERIFIER_nondet_int();

int fixedFun() {
	return 5;
}

int main() {
	char string[] = "Hello world!";
	//int off = __VERIFIER_nondet_int();
	assert(false);
	int off = fixedFun();

	if(off > sizeof(string))
		return 0;

	char* p = string + off;

	return *p;
}