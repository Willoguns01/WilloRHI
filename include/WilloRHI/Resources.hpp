#pragma once

#include <stdint.h>

#include "WilloRHI/Types.hpp"

namespace WilloRHI
{
    struct BufferId    { uint64_t id = 0; };
    struct ImageId     { uint64_t id = 0; };
    struct ImageViewId { uint64_t id = 0; };
    struct SamplerId   { uint64_t id = 0; };

    // createinfo structures
    struct BufferCreateInfo {
        uint32_t size = 0;
        AllocationUsageFlags usage;
    };

    struct ImageCreateInfo {
        
    };

    struct ImageViewCreateInfo {

    };

    struct SamplerCreateInfo {

    };
}
