#pragma once

#include <stdint.h>

#include "WilloRHI/Types.hpp"

namespace WilloRHI
{
    struct ResourceId {
        uint64_t id = 0;
    };

    struct BufferId : public ResourceId {};
    struct ImageId : public ResourceId {};
    struct ImageViewId : public ResourceId {};
    struct SamplerId : public ResourceId {};

    // createinfo structures
    struct BufferCreateInfo {
        uint64_t size = 0;
        AllocationFlags allocationFlags;
    };

    struct ImageCreateInfo {
        
    };

    struct ImageViewCreateInfo {

    };

    struct SamplerCreateInfo {

    };
}
