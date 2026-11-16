#include "mathlib.h"

int32_t  math_factorial(int32_t n){
    if(n < 0) {
        return -1; // Error: factorial is not defined for negative numbers
    }
    if(n == 0) {
        return 1;
    }
    return (n == 0) ? 1 : n * math_factorial(n - 1);
}


int32_t  math_gcd(int32_t a, int32_t b){
    if (b == 0) {
        return a;
    }
    return math_gcd(b, a % b);
}

uint32_t math_isqrt(uint32_t n) {
    uint32_t x = n;
    uint32_t y = (x + 1) / 2;
    while (y < x) {
        x = y;
        y = (x + n / x) / 2;
    }
    return x;
}

int math_is_prime(int32_t n) {
    if (n <= 1) {
        return 0; // Not prime
    }
    for (int32_t i = 2; i * i <= n; i++) {
        if (n % i == 0) {
            return 0; // Not prime
        }
    }
    return 1; // Prime
}