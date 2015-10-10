#include <cstdlib>
#include <string.h>

int __VERIFIER_nondet_int();

void insert(char* str) {
	strcpy(str, "Hello, world!");
}

int main() {
	char buffer[32];
	insert(buffer);
	return 0;
}