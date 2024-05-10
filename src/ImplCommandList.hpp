#pragma once

#include "WilloRHI/CommandList.hpp"
#include "WilloRHI/Device.hpp"

#include "ImplResources.hpp"

#include <vulkan/vulkan.h>

namespace WilloRHI
{
    struct ImplCommandList
    {
        Device _device;

        VkCommandBuffer _vkCommandBuffer = VK_NULL_HANDLE;
        std::thread::id _threadId;

        DeletionQueues _deletionQueues;
        DeviceResources* _resources = nullptr;

        void Init();

        void Begin();
        void End();

        void ClearImage(ImageId image, const float clearColour[4], const ImageSubresourceRange& subresourceRange);

        void TransitionImageLayout(ImageId image, const ImageMemoryBarrierInfo& barrierInfo);
    
        void CopyImage(ImageId srcImage, ImageId dstImage, uint32_t numRegions, ImageCopyRegion* regions);
        void CopyBufferToImage(BufferId srcBuffer, ImageId dstImage, uint32_t numRegions, BufferImageCopyRegion* regions);
        void CopyBuffer(BufferId srcBuffer, BufferId dstBuffer, uint32_t numRegions, BufferCopyRegion* regions);

        void DestroyBuffer(BufferId buffer);
        void DestroyImage(ImageId image);
        void DestroyImageView(ImageViewId imageView);
        void DestroySampler(SamplerId sampler);

        void* GetNativeHandle() const;
        std::thread::id GetThreadId() const;
        void* GetDeletionQueue();
    };
}
