#include <cstdlib>

int __VERIFIER_nondet_int();
int process_array(int*);

int alloc_static_const() {
	int array[10];
	for (int i = 0; i != 10; i++) {
		array[i] = i;
	}
	return process_array(array);
}

int alloc_malloc_const() {
	int* array = (int*) malloc(10 * sizeof(int));
	for (int i = 0; i != 10; i++) {
		array[i] = i;
	}
	return process_array(array);
}

int alloc_new_const() {
	int* array = new int[10];
	for (int i = 0; i != 10; i++) {
		array[i] = i;
	}
	return process_array(array);
}

int main() {
	return 0;
}