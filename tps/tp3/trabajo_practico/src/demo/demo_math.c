
#include "demo.h"
#include "mathlib.h"
#include "print.h"

void demo_math(void)
{
    print_str("Hello, World!\n");
    print_str("factorial(5) = ");
    print_dec(math_factorial(5));
    print_str("\n");
    print_str("gcd(48, 18) = ");
    print_dec(math_gcd(48, 18));
    print_str("\n");
    print_dec(math_isqrt(17));
    print_str("\n");
    print_dec(math_is_prime(17));
    print_str("\n");
    print_dec(math_is_prime(18));
    print_str("\n");
}
