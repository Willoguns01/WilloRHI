#pragma once

#include "WilloRHI/CommandList.hpp"
#include <vulkan/vulkan.h>

namespace WilloRHI
{
    struct ImplCommandList
    {
        ImplDevice* _device = nullptr;

        VkCommandBuffer _vkCommandBuffer = VK_NULL_HANDLE;
        std::thread::id _threadId;

        void Reset();

        void Begin();
        void End();

        void ClearImage(ImageId image, const float clearColour[4], const ImageSubresourceRange& subresourceRange);

        void TransitionImageLayout(ImageId image, const ImageMemoryBarrierInfo& barrierInfo);
    };
}
