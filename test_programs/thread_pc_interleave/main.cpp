#include <pthread.h>

int __VERIFIER_nondet_int();

volatile int glob = 0;


void* f1(void*) {
	do {
		if(glob < 5)
			glob = __VERIFIER_nondet_int();
		else
			glob = __VERIFIER_nondet_int() / 2;
	} while(glob < 25);
	return 0;
}

void* f2(void*) {
	do {
		glob = __VERIFIER_nondet_int();
	} while(glob < 30);
	return 0;
}

int main() {
	pthread_t t1, t2;

	pthread_create(&t1, NULL, f1, 0);
	pthread_create(&t2, NULL, f2, 0);

	pthread_join(t1, NULL);
	pthread_join(t2, NULL);
}