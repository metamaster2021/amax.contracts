#pragma once

//#include <math.h>

static constexpr int128_t HIGH_PRECISION_1 = 100000000000000000;   //17*0 behind 1
static constexpr int128_t PRECISION_1      = 10000;                // 4*0 behind 1
static constexpr int128_t PRECISION        = 4;

namespace wasm { namespace safemath {
    template<typename T>
    uint128_t divide_decimal(uint128_t a, uint128_t b, T precision) {
        uint128_t tmp = 10 * a * precision  / b;
        return (tmp + 5) / 10;
    }

    template<typename T>
    uint128_t multiply_decimal(uint128_t a, uint128_t b, T precision) {
        uint128_t tmp = 10 * a * b / precision;
        return (tmp + 5) / 10;
    }


    #define div(a, b) divide_decimal(a, b, PRECISION_1)
    #define mul(a, b) multiply_decimal(a, b, PRECISION_1)

    #define high_div(a, b) divide_decimal(a, b, HIGH_PRECISION_1)
    #define high_mul(a,b ) multiply_decimal(a, b, HIGH_PRECISION_1)


    //https://mpark.github.io/programming/2014/08/18/exponentiation-by-squaring/
    inline uint128_t pow_decimal(uint128_t x, uint128_t n) {
        uint128_t high_precision_x = x * HIGH_PRECISION_1 / PRECISION_1;
        uint128_t result = HIGH_PRECISION_1;
        while(n > 0) {
            if (n % 2 != 0) {
                result = high_mul(result, high_precision_x);
            }
            high_precision_x = high_mul(high_precision_x, high_precision_x);
            n /= 2;
        }
        return result * PRECISION_1 / HIGH_PRECISION_1;
    }

} } //safemath
