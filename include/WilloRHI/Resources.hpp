#pragma once

#include <stdint.h>

#include "WilloRHI/Types.hpp"

namespace WilloRHI
{
    typedef uint64_t BufferId;
    typedef uint64_t ImageId;
    typedef uint64_t ImageViewId;
    typedef uint64_t SamplerId;

    // createinfo structures
    struct BufferCreateInfo {
        uint64_t size = 0;
        AllocationUsageFlags allocationFlags = {};
    };

    struct ImageCreateInfo {
        uint32_t dimensions = 2;
        Extent3D size = {0, 0, 0};
        uint32_t numLevels = 0;
        uint32_t numLayers = 0;
        Format format = Format::UNDEFINED;
        ImageUsageFlags usageFlags = {};
        ImageCreateFlags createFlags = {};
        AllocationUsageFlags allocationFlags = {};
        ImageTiling tiling = ImageTiling::OPTIMAL;
    };

    struct ImageViewCreateInfo {

    };

    struct SamplerCreateInfo {

    };
}
