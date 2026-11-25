/*
 * udiv64.c -- core A's 64-bit division, in place of libgcc's: the compiler's __aeabi_uldivmod and
 * __aeabi_ldivmod (libgcc, small) call __udivmoddi4, and libgcc's is 754 bytes of fast code. This one
 * is 170: the CPU's own 32-bit divide when both numbers fit in 32 bits, else a shift and subtract a
 * bit. Core A divides 64-bit numbers only a few times a track or a poll (durations, positions, the
 * clock's self-test), never in a loop, so its speed does not matter (2026-11-25, the size budget).
 * Host test: firmware/libc/tests/test_udiv64.c (make host-tests).
 */
#include <stdint.h>

uint64_t __udivmoddi4(uint64_t n, uint64_t d, uint64_t *rem);

uint64_t __udivmoddi4(uint64_t n, uint64_t d, uint64_t *rem)
{
    uint64_t q = 0, r = 0;

    if (!d) {                            /* (no trap, as libgcc's: callers never divide by 0) */
        if (rem)
            *rem = n;
        return ~(uint64_t)0;
    }
    if (!(n >> 32) && !(d >> 32)) {      /* the usual case: the CPU's own divide */
        uint32_t a = (uint32_t)n, b = (uint32_t)d;

        if (rem)
            *rem = a % b;
        return a / b;
    }
    for (int i = 63; i >= 0; i--) {
        r = r << 1 | (n >> i & 1u);
        if (r >= d) {
            r -= d;
            q |= (uint64_t)1 << i;
        }
    }
    if (rem)
        *rem = r;
    return q;
}
