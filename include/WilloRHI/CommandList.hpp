#pragma once

#include "WilloRHI/Forward.hpp"
#include "WilloRHI/Types.hpp"
#include "WilloRHI/Resources.hpp"

#include <stdint.h>
#include <memory>
#include <optional>

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

    struct DrawIndirectCommand
    {
        uint32_t vertexCount;
        uint32_t instanceCount;
        uint32_t firstVertex;
        uint32_t firstInstance;
    };

    struct DrawIndexedIndirectCommand
    {
        uint32_t indexCount;
        uint32_t instanceCount;
        uint32_t firstIndex;
        int32_t vertexOffset;
        uint32_t firstInstance;
    };

    struct RenderPassAttachmentInfo
    {
        ImageViewId imageView = 0;
        ImageLayout imageLayout = ImageLayout::UNDEFINED;
        LoadOp loadOp = LoadOp::LOAD;
        StoreOp storeOp = StoreOp::STORE;
        ClearColour clearColour = {};
    };

    struct RenderPassBeginInfo
    {
        std::vector<RenderPassAttachmentInfo> colourAttachments;
        std::optional<RenderPassAttachmentInfo> depthAttachment = {};
        Rect2D renderingArea = {};
    };

    class CommandList
    {
    public:
        CommandList() = default;

        void Begin();
        void End();

        void BeginRendering(const RenderPassBeginInfo& beginInfo);
        void EndRendering();

        void PushConstants(uint32_t offset, uint32_t size, void* data);

        // barriers
        void GlobalMemoryBarrier(const GlobalMemoryBarrierInfo& barrierInfo);
        void ImageMemoryBarrier(ImageId image, const ImageMemoryBarrierInfo& barrierInfo);
        void BufferMemoryBarrier(BufferId buffer, const BufferMemoryBarrierInfo& barrierInfo);

        void FlushBarriers();

        // pipelines
        void BindComputePipeline(ComputePipeline pipeline);
        void BindGraphicsPipeline(GraphicsPipeline pipeline);

        void BindVertexBuffer(BufferId buffer, uint32_t binding);
        void BindIndexBuffer(BufferId buffer, uint64_t bufferOffset, IndexType indexType);

        void SetViewport(Viewport viewport);
        void SetScissor(std::vector<Rect2D> scissor);

        // compute dispatch
        void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);

        // drawing commands
        void ClearImage(ImageId image, ClearColour clearColour, const ImageSubresourceRange& subresourceRange);

        void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
        void DrawIndirect(BufferId argBuffer, uint64_t offset, uint32_t drawCount);
        void DrawIndirectCount(BufferId argBuffer, uint64_t offset, BufferId countBuffer, uint64_t countBufferOffset, uint32_t maxDrawCount);

        void DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t vertexOffset, uint32_t firstInstance);
        void DrawIndexedIndirect(BufferId argBuffer, uint64_t offset, uint32_t drawCount);
        void DrawIndexedIndirectCount(BufferId argBuffer, uint64_t offset, BufferId countBuffer, uint64_t countBufferOffset, uint32_t maxDrawCount);

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
