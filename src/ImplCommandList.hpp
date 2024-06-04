#pragma once

#include "WilloRHI/CommandList.hpp"
#include "WilloRHI/Device.hpp"
#include "WilloRHI/Pipeline.hpp"

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

        VkPipelineBindPoint _currentPipeline = {};
        VkPipelineLayout _currentPipelineLayout = VK_NULL_HANDLE;

        std::vector<VkMemoryBarrier2> _globalBarriers;
        std::vector<VkBufferMemoryBarrier2> _bufferBarriers;
        std::vector<VkImageMemoryBarrier2> _imageBarriers;

        void Init();

        void Begin();
        void End();

        void PushConstants(uint32_t offset, uint32_t size, void* data);

        // barriers
        void GlobalMemoryBarrier(const GlobalMemoryBarrierInfo& barrierInfo);
        void ImageMemoryBarrier(ImageId image, const ImageMemoryBarrierInfo& barrierInfo);
        void BufferMemoryBarrier(BufferId buffer, const BufferMemoryBarrierInfo& barrierInfo);

        void FlushBarriers();

        // pipelines
        void BindComputePipeline(ComputePipeline pipeline);
        void BindGraphicsPipeline(GraphicsPipeline pipeline);

        // compute dispatch
        void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);

        // drawing commands
        void ClearImage(ImageId image, const float clearColour[4], const ImageSubresourceRange& subresourceRange);

        // copy commands
        void CopyImage(ImageId srcImage, ImageId dstImage, uint32_t numRegions, ImageCopyRegion* regions);
        void BlitImage(ImageId srcImage, ImageId dstImage, Filter filter);
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
