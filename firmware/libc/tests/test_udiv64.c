/*
 * test_udiv64.c -- firmware/libc/udiv64.c's __udivmoddi4 (built here as my_udivmoddi4: the host's own
 * division stays the compiler's) against the host's 64-bit / and %: random numbers of every size and
 * the edges.
 */
#include <stdint.h>
#include <stdio.h>

uint64_t my_udivmoddi4(uint64_t n, uint64_t d, uint64_t *rem);

static uint64_t s = 88172645463325252ull;

static uint64_t rnd(void)                /* xorshift64 */
{
    s ^= s << 13;
    s ^= s >> 7;
    s ^= s << 17;
    return s;
}

static unsigned long bad, n;

static void check(uint64_t a, uint64_t b)
{
    uint64_t r, q = my_udivmoddi4(a, b, &r);

    n++;
    if ((q != a / b || r != a % b) && bad++ < 5)
        printf("wrong: %llu / %llu = %llu r %llu\n", (unsigned long long)a, (unsigned long long)b,
               (unsigned long long)q, (unsigned long long)r);
}

int main(void)
{
    static const uint64_t e[] = { 0, 1, 2, 3, 0xFFFFFFFFull, 0x100000000ull, 0x100000001ull,
                                  0x7FFFFFFFFFFFFFFFull, 0x8000000000000000ull, 0xFFFFFFFFFFFFFFFFull };
    const unsigned ne = sizeof e / sizeof e[0];

    for (int i = 0; i < 2000000; i++) {
        uint64_t a = rnd() >> (rnd() % 64), b = rnd() >> (rnd() % 64);

        check(a, b ? b : 1);
    }
    for (unsigned i = 0; i < ne; i++)
        for (unsigned j = 1; j < ne; j++)
            check(e[i], e[j]);
    printf("test_udiv64: %lu divisions, %lu wrong\n", n, bad);
    return bad != 0;
}
