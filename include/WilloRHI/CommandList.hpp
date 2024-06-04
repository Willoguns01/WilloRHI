#pragma once

#include "WilloRHI/Forward.hpp"
#include "WilloRHI/Types.hpp"
#include "WilloRHI/Resources.hpp"

#include <stdint.h>
#include <memory>

namespace WilloRHI
{
    struct GlobalMemoryBarrierInfo
    {
        PipelineStageFlags srcStage = PipelineStageFlag::NONE;
        PipelineStageFlags dstStage = PipelineStageFlag::NONE;
        MemoryAccessFlags srcAccess = MemoryAccessFlag::NONE;
        MemoryAccessFlags dstAccess = MemoryAccessFlag::NONE;
    };

    struct ImageMemoryBarrierInfo
    {
        PipelineStageFlags dstStage = PipelineStageFlag::NONE;
        MemoryAccessFlags dstAccess = MemoryAccessFlag::NONE;
        ImageLayout dstLayout = ImageLayout::UNDEFINED;
        ImageSubresourceRange subresourceRange = {};
    };

    struct BufferMemoryBarrierInfo
    {
        PipelineStageFlags dstStage = PipelineStageFlag::NONE;
        MemoryAccessFlags dstAccess = MemoryAccessFlag::NONE;
    };

    struct ImageCopyRegion
    {
        ImageSubresourceLayers srcSubresource = {};
        Offset3D srcOffset = {};
        ImageSubresourceLayers dstSubresource = {};
        Offset3D dstOffset = {};
        Extent3D extent = {};
    };

    struct BufferImageCopyRegion
    {
        uint64_t bufferOffset = 0;
        uint32_t rowLength = 0;
        uint32_t imageHeight = 0;
        ImageSubresourceLayers dstSubresource = {};
        Offset3D dstOffset = {};
        Extent3D extent = {};
    };

    struct BufferCopyRegion
    {
        uint64_t srcOffset = 0;
        uint64_t dstOffset = 0;
        uint64_t size = 0;
    };

    class CommandList
    {
    public:
        CommandList() = default;

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

    protected:
        friend ImplDevice;
        friend ImplQueue;
        std::shared_ptr<ImplCommandList> impl = nullptr;

        CommandList(Device device, std::thread::id threadId, void* nativeHandle);

        void* GetNativeHandle() const;
        std::thread::id GetThreadId() const;
        void* GetDeletionQueue();
    };
}
