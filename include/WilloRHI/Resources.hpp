#pragma once

#include <stdint.h>

#include "WilloRHI/Types.hpp"

namespace WilloRHI
{
    typedef uint32_t BufferId;
    typedef uint32_t ImageId;
    typedef uint32_t ImageViewId;
    typedef uint32_t SamplerId;

    constexpr float LOD_CLAMP_NONE  = 1000.0F;

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
        ImageId image = 0;
        ImageViewType viewType = ImageViewType::VIEW_TYPE_2D;
        Format format = Format::UNDEFINED;
        ImageSubresourceRange subresource = {};
    };

    struct SamplerCreateInfo {
        Filter magFilter = Filter::LINEAR;
        Filter minFilter = Filter::LINEAR;
        Filter mipFilter = Filter::LINEAR;
        ReductionMode reductionMode = ReductionMode::WEIGHTED_AVERAGE;
        SamplerAddressMode addressModeU = SamplerAddressMode::CLAMP_EDGE;
        SamplerAddressMode addressModeV = SamplerAddressMode::CLAMP_EDGE;
        SamplerAddressMode addressModeW = SamplerAddressMode::CLAMP_EDGE;
        float lodBias = 0.0f;
        bool anisotropyEnable = false;
        float maxAnisotropy = 0.0f;
        bool compareOpEnable = false;
        CompareOp compareOp = CompareOp::ALWAYS;
        float minLod = 0.0f;
        float maxLod = LOD_CLAMP_NONE;
        BorderColour borderColour = BorderColour::FLOAT_TRANSPARENT_BLACK;
        bool unnormalizedCoordinates = false;
    };
}
