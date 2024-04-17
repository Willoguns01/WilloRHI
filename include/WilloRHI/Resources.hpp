#pragma once

#include <stdint.h>

#include "WilloRHI/Types.hpp"

namespace WilloRHI
{
    struct AllocationCreateInfo {
        uint32_t size = 0 ;
    };

    struct BufferId    { uint64_t id = 0; };
    struct ImageId     { uint64_t id = 0; };
    struct ImageViewId { uint64_t id = 0; };
    struct SamplerId   { uint64_t id = 0; };

    // createinfo structures
    struct BufferCreateInfo {
        AllocationCreateInfo allocationInfo = {};
        AllocationFlags allocationFlags;
    };

    struct ImageCreateInfo {
        
    };

    struct ImageViewCreateInfo {

    };

    struct SamplerCreateInfo {

    };
}
