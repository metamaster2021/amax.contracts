#pragma once

//#include <math.h>
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

#define div(a, b, p) divide_decimal     (a, b, p)
#define mul(a, b, p) multiply_decimal   (a, b, p)

} } //safemath