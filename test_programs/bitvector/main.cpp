extern int __VERIFIER_nondet_int(void);
extern void __VERIFIER_assert(bool);
/* emulates multi-precision addition */
#include <assert.h>

unsigned int mp_add(unsigned int a, unsigned int b)
{
    unsigned char a0, a1, a2, a3;
    unsigned char b0, b1, b2, b3;
    unsigned char r0, r1, r2, r3;

    unsigned short carry;
    unsigned short partial_sum;
    unsigned int r;
    unsigned char i;
    unsigned char na, nb;

    a0 = a;
    a1 = a >> 8;
    a2 = a >> 16U;
    a3 = a >> 24U;

    b0 = b;
    b1 = b >> 8U;
    b2 = b >> 16U;
    b3 = b >> 24U;

    na = (unsigned char)4; /* num of components of a */
    if (a3 == (unsigned char)0) {
        na = na - 1;
        if (a2 == (unsigned char)0) {
            na = na - 1;
            if (a1 == (unsigned char)0) {
                na = na - 1;
            }
        }
    }

    nb = (unsigned char)4; /* num of components of b */
    if (b3 == (unsigned char)0) {
        nb = nb - 1;
        if (b2 == (unsigned char)0) {
            nb = nb - 1;
            if (b1 == (unsigned char)0) {
                nb = nb - 1;
            }
        }
    }
    
    carry = (unsigned short)0;
    i = (unsigned char)0;
    while ((i < na) || (i < nb) || (carry != (unsigned short)0)) {
        partial_sum = carry;
        carry = (unsigned short)0;

        if (i < na) {
            if (i == (unsigned char)0) { partial_sum = partial_sum + a0; }
            if (i == (unsigned char)1) { partial_sum = partial_sum + a1; }
            if (i == (unsigned char)2) { partial_sum = partial_sum + a2; }
            if (i == (unsigned char)3) { partial_sum = partial_sum + a3; }
        }

        i = i + (unsigned char)1;
    }

    r = r0 | (r1 << 8U) | (r2 << 16U) | (r3 << 24U);

    return r;
}


int main()
{
    unsigned int a, b = __VERIFIER_nondet_int(), r, r2;

    a = 234770789;

    r = mp_add(a, b);
    r2 = mp_add(a + 1, b - 1);

   __VERIFIER_assert(r == r2);
    
    return 0;
}
