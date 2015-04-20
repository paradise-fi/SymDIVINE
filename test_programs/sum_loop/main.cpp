int __VERIFIER_nondet_int();
void __VERIFIER_assert(bool);

int main() {
	unsigned int a = __VERIFIER_nondet_int();
	unsigned int sum = 0;
	for (unsigned int i = 0; i != a; i++) {
		sum += i;
	}
	//__VERIFIER_assert(sum == a * (1 + a) / 2);
	return sum;
}