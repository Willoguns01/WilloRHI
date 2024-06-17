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
        VkPipelineStageFlags _currentStageFlags = VK_PIPELINE_STAGE_FLAG_BITS_MAX_ENUM;

        std::vector<VkMemoryBarrier2> _globalBarriers;
        std::vector<VkBufferMemoryBarrier2> _bufferBarriers;
        std::vector<VkImageMemoryBarrier2> _imageBarriers;

        void Init();

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

        void* GetNativeHandle() const;
        std::thread::id GetThreadId() const;
        void* GetDeletionQueue();
    };
}
