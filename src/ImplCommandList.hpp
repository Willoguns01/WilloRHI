#pragma once

#include "WilloRHI/CommandList.hpp"
#include "ImplDevice.hpp"
#include <vulkan/vulkan.h>

namespace WilloRHI
{
    struct ImplCommandList
    {
        Device _device;

        VkCommandBuffer _vkCommandBuffer = VK_NULL_HANDLE;
        std::thread::id _threadId;

        DeletionQueues _deletionQueues;

        void Reset();

        void Begin();
        void End();

        void ClearImage(ImageId image, const float clearColour[4], const ImageSubresourceRange& subresourceRange);

        void TransitionImageLayout(ImageId image, const ImageMemoryBarrierInfo& barrierInfo);
    };
}
