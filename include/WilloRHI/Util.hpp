#pragma once

#include <stdint.h>

namespace WilloRHI::Util
{
    inline constexpr int64_t Power(uint64_t input, uint64_t level) {
        uint64_t temp = input;
        for (int i = 0; i < level - 1; i++) {
            temp = temp * input;
        }
        return temp;
    }
}
