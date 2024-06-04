#include "ImplCommandList.hpp"

namespace WilloRHI
{
    void ImplCommandList::Init() {
        _resources = static_cast<DeviceResources*>(_device.GetDeviceResources());
    }

    void CommandList::Begin() { impl->Begin(); }
    void ImplCommandList::Begin()
    {
        _device.LockResources_Shared();

        VkCommandBufferBeginInfo beginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = nullptr
        };

        vkBeginCommandBuffer(_vkCommandBuffer, &beginInfo);
    }

    void CommandList::End() { impl->End(); }
    void ImplCommandList::End()
    {
        _device.UnlockResources_Shared();
        vkEndCommandBuffer(_vkCommandBuffer);
    }

    void CommandList::PushConstants(uint32_t offset, uint32_t size, void* data) {
        impl->PushConstants(offset, size, data); }
    void ImplCommandList::PushConstants(uint32_t offset, uint32_t size, void* data) {
        vkCmdPushConstants(_vkCommandBuffer, _currentPipelineLayout, VK_SHADER_STAGE_ALL, offset, size, data);
    }

    void CommandList::TransitionImageLayout(ImageId image, const ImageMemoryBarrierInfo& barrierInfo) {
        impl->TransitionImageLayout(image, barrierInfo); }
    // TODO: barriers should be piled together and flushed all at once when necessary
    void ImplCommandList::TransitionImageLayout(ImageId image, const ImageMemoryBarrierInfo& barrierInfo)
    {
        VkImageSubresourceRange resourceRange = {
            .aspectMask = _resources->images.At(image).aspect,
            .baseMipLevel = barrierInfo.subresourceRange.baseLevel,
            .levelCount = barrierInfo.subresourceRange.numLevels,
            .baseArrayLayer = barrierInfo.subresourceRange.baseLayer,
            .layerCount = barrierInfo.subresourceRange.numLayers
        };

        ImageResource& imageResource = _resources->images.At(image);
        VkImage vkImage = imageResource.image;

        VkImageMemoryBarrier2 imageBarrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .pNext = nullptr,
            .srcStageMask = static_cast<VkPipelineStageFlagBits2>(imageResource.currentPipelineStage),
            .srcAccessMask = static_cast<VkAccessFlagBits2>(imageResource.currentAccessFlags),
            .dstStageMask = static_cast<VkPipelineStageFlagBits2>(barrierInfo.dstStage),
            .dstAccessMask = static_cast<VkAccessFlagBits2>(barrierInfo.dstAccess),
            .oldLayout = static_cast<VkImageLayout>(imageResource.currentLayout),
            .newLayout = static_cast<VkImageLayout>(barrierInfo.dstLayout),
            .image = vkImage,
            .subresourceRange = resourceRange
        };

        VkDependencyInfo depInfo = {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext = nullptr,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &imageBarrier,
        };

        vkCmdPipelineBarrier2(_vkCommandBuffer, &depInfo);

        imageResource.currentLayout = static_cast<VkImageLayout>(barrierInfo.dstLayout);
        imageResource.currentAccessFlags = static_cast<VkAccessFlagBits2>(barrierInfo.dstAccess);
        imageResource.currentPipelineStage = static_cast<VkPipelineStageFlagBits2>(barrierInfo.dstStage);
    }

    void CommandList::BindComputePipeline(ComputePipeline pipeline) {
        impl->BindComputePipeline(pipeline); }
    void ImplCommandList::BindComputePipeline(ComputePipeline pipeline) {
        _currentPipeline = VK_PIPELINE_BIND_POINT_COMPUTE;
        _currentPipelineLayout = static_cast<VkPipelineLayout>(pipeline.GetPipelineLayout());
        vkCmdBindDescriptorSets(
            _vkCommandBuffer,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            static_cast<VkPipelineLayout>(pipeline.GetPipelineLayout()),
            0, 1,
            &static_cast<GlobalDescriptors*>(_device.GetResourceDescriptors())->descriptorSet,
            0, nullptr
        );
        vkCmdBindPipeline(_vkCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, static_cast<VkPipeline>(pipeline.GetPipelineHandle()));
    }

    void CommandList::BindGraphicsPipeline(GraphicsPipeline pipeline) {
        impl->BindGraphicsPipeline(pipeline); }
    void ImplCommandList::BindGraphicsPipeline(GraphicsPipeline pipeline) {
        _currentPipeline = VK_PIPELINE_BIND_POINT_GRAPHICS;
        _currentPipelineLayout = static_cast<VkPipelineLayout>(pipeline.GetPipelineLayout());
        vkCmdBindDescriptorSets(
            _vkCommandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            static_cast<VkPipelineLayout>(pipeline.GetPipelineLayout()),
            0, 1,
            &static_cast<GlobalDescriptors*>(_device.GetResourceDescriptors())->descriptorSet,
            0, nullptr
        );
        vkCmdBindPipeline(_vkCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, static_cast<VkPipeline>(pipeline.GetPipelineHandle()));
    }

    void CommandList::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
        impl->Dispatch(groupCountX, groupCountY, groupCountZ); }
    void ImplCommandList::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
    {
        vkCmdDispatch(_vkCommandBuffer, groupCountX, groupCountY, groupCountZ);
    }

    void CommandList::ClearImage(ImageId image, const float clearColour[4], const ImageSubresourceRange& subresourceRange) {
        impl->ClearImage(image, clearColour, subresourceRange); }
    void ImplCommandList::ClearImage(ImageId image, const float clearColour[4], const ImageSubresourceRange& subresourceRange)
    {
        VkClearColorValue vkClear = { {clearColour[0], clearColour[1], clearColour[2], clearColour[3]} };

        VkImageSubresourceRange resourceRange = {
            .aspectMask = _resources->images.At(image).aspect,
            .baseMipLevel = subresourceRange.baseLevel,
            .levelCount = subresourceRange.numLevels,
            .baseArrayLayer = subresourceRange.baseLayer,
            .layerCount = subresourceRange.numLayers
        };

        VkImage vkImage = static_cast<VkImage>(_device.GetImageNativeHandle(image));
        vkCmdClearColorImage(_vkCommandBuffer, vkImage, VK_IMAGE_LAYOUT_GENERAL, &vkClear, 1, &resourceRange);
    }

    void CommandList::CopyImage(ImageId srcImage, ImageId dstImage, uint32_t numRegions, ImageCopyRegion* regions) {
        impl->CopyImage(srcImage, dstImage, numRegions, regions); }
    void ImplCommandList::CopyImage(ImageId srcImage, ImageId dstImage, uint32_t numRegions, ImageCopyRegion* regions) 
    {
        ImageResource& srcResource = _resources->images.At(srcImage);
        ImageResource& dstResource = _resources->images.At(dstImage);

        // TODO: test performance std::vector.resize vs. alloc on heap
        //VkImageCopy* vkRegions = new VkImageCopy[numRegions];
        std::vector<VkImageCopy> vkRegions;
        vkRegions.resize(numRegions);

        for (uint32_t i = 0; i < numRegions; i++) {
            vkRegions[i] = {
                .srcSubresource = {
                    .aspectMask = srcResource.aspect,
                    .mipLevel = regions[i].srcSubresource.level,
                    .baseArrayLayer = regions[i].srcSubresource.baseLayer,
                    .layerCount = regions[i].srcSubresource.numLayers
                },
                .srcOffset = {regions[i].srcOffset.x,regions[i].srcOffset.y,regions[i].srcOffset.z},
                .dstSubresource = {
                    .aspectMask = dstResource.aspect,
                    .mipLevel = regions[i].dstSubresource.level,
                    .baseArrayLayer = regions[i].dstSubresource.baseLayer,
                    .layerCount = regions[i].dstSubresource.numLayers
                },
                .dstOffset = {regions[i].dstOffset.x,regions[i].dstOffset.y,regions[i].dstOffset.z},
                .extent = {regions[i].extent.width,regions[i].extent.height,regions[i].extent.depth,}
            };
        }

        vkCmdCopyImage(_vkCommandBuffer, 
            srcResource.image, srcResource.currentLayout,
            dstResource.image, dstResource.currentLayout,
            numRegions, vkRegions.data());
    }

    void CommandList::BlitImage(ImageId srcImage, ImageId dstImage, Filter filter) {
        impl->BlitImage(srcImage, dstImage, filter); }
    void ImplCommandList::BlitImage(ImageId srcImage, ImageId dstImage, Filter filter)
    {
        ImageResource& srcResource = _resources->images.At(srcImage);
        ImageResource& dstResource = _resources->images.At(dstImage);

        VkImageBlit2 blitRegion = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
            .pNext = nullptr,
            .srcSubresource = {
                .aspectMask = srcResource.aspect,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1
            },
            .dstSubresource = {
                .aspectMask = dstResource.aspect,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };
        blitRegion.srcOffsets[1].x = srcResource.createInfo.size.width;
        blitRegion.srcOffsets[1].y = srcResource.createInfo.size.height;
        blitRegion.srcOffsets[1].z = 1;
        blitRegion.dstOffsets[1].x = dstResource.createInfo.size.width;
        blitRegion.dstOffsets[1].y = dstResource.createInfo.size.height;
        blitRegion.dstOffsets[1].z = 1;

        VkBlitImageInfo2 blitInfo = {
            .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
            .pNext = nullptr,
            .srcImage = srcResource.image,
            .srcImageLayout = srcResource.currentLayout,
            .dstImage = dstResource.image,
            .dstImageLayout = dstResource.currentLayout,
            .regionCount = 1,
            .pRegions = &blitRegion,
            .filter = static_cast<VkFilter>(filter)
        };

        vkCmdBlitImage2(_vkCommandBuffer, &blitInfo);
    }

    void CommandList::CopyBufferToImage(BufferId srcBuffer, ImageId dstImage, uint32_t numRegions, BufferImageCopyRegion* regions) {
        impl->CopyBufferToImage(srcBuffer, dstImage, numRegions, regions); }
    void ImplCommandList::CopyBufferToImage(BufferId srcBuffer, ImageId dstImage, uint32_t numRegions, BufferImageCopyRegion* regions) 
    {
        BufferResource& srcResource = _resources->buffers.At(srcBuffer);
        ImageResource& dstResource = _resources->images.At(dstImage);

        std::vector<VkBufferImageCopy> vkRegions;
        vkRegions.resize(numRegions);

        for (uint32_t i = 0; i < numRegions; i++) {
            vkRegions[i] = {
                .bufferOffset = regions[i].bufferOffset,
                .bufferRowLength = regions[i].rowLength,
                .bufferImageHeight = regions[i].imageHeight,
                .imageSubresource = {
                    .aspectMask = dstResource.aspect,
                    .mipLevel = regions[i].dstSubresource.level,
                    .baseArrayLayer = regions[i].dstSubresource.baseLayer,
                    .layerCount = regions[i].dstSubresource.numLayers
                },
                .imageOffset = {regions[i].dstOffset.x, regions[i].dstOffset.y, regions[i].dstOffset.z},
                .imageExtent = {regions[i].extent.width, regions[i].extent.height, regions[i].extent.depth}
            };
        }

        vkCmdCopyBufferToImage(_vkCommandBuffer, 
            srcResource.buffer, dstResource.image, dstResource.currentLayout,
            numRegions, vkRegions.data());
    }

    void CommandList::CopyBuffer(BufferId srcBuffer, BufferId dstBuffer, uint32_t numRegions, BufferCopyRegion* regions) {
        impl->CopyBuffer(srcBuffer, dstBuffer, numRegions, regions); }
    void ImplCommandList::CopyBuffer(BufferId srcBuffer, BufferId dstBuffer, uint32_t numRegions, BufferCopyRegion* regions)
    {
        BufferResource& srcResource = _resources->buffers.At(srcBuffer);
        BufferResource& dstResource = _resources->buffers.At(dstBuffer);

        std::vector<VkBufferCopy> vkRegions;
        vkRegions.resize(numRegions);

        for (uint32_t i = 0; i < numRegions; i++) {
            vkRegions[i] = {
                .srcOffset = regions[i].srcOffset,
                .dstOffset = regions[i].dstOffset,
                .size = regions[i].size
            };
        }
    }

    void CommandList::DestroyBuffer(BufferId buffer) { impl->DestroyBuffer(buffer); }
    void ImplCommandList::DestroyBuffer(BufferId buffer) {
        _deletionQueues.bufferQueue.enqueue(buffer);
    }
    
    void CommandList::DestroyImage(BufferId buffer) { impl->DestroyImage(buffer); }
    void ImplCommandList::DestroyImage(BufferId buffer) {
        _deletionQueues.imageQueue.enqueue(buffer);
    }

    void CommandList::DestroyImageView(BufferId buffer) { impl->DestroyImageView(buffer); }
    void ImplCommandList::DestroyImageView(BufferId buffer) {
        _deletionQueues.imageViewQueue.enqueue(buffer);
    }

    void CommandList::DestroySampler(BufferId buffer) { impl->DestroySampler(buffer); }
    void ImplCommandList::DestroySampler(BufferId buffer) {
        _deletionQueues.samplerQueue.enqueue(buffer);
    }

    void* CommandList::GetNativeHandle() const { return impl->GetNativeHandle(); }
    void* ImplCommandList::GetNativeHandle() const {
        return static_cast<void*>(_vkCommandBuffer);
    }

    std::thread::id CommandList::GetThreadId() const { return impl->GetThreadId(); }
    std::thread::id ImplCommandList::GetThreadId() const {
        return _threadId;
    }

    void* CommandList::GetDeletionQueue() { return impl->GetDeletionQueue(); }
    void* ImplCommandList::GetDeletionQueue() {
        return &_deletionQueues;
    }

    CommandList::CommandList(Device device, std::thread::id threadId, void* nativeHandle) {
        impl = std::make_shared<ImplCommandList>();
        impl->_device = device;
        impl->_threadId = threadId;
        impl->_vkCommandBuffer = (VkCommandBuffer)nativeHandle;
        impl->Init();
    }
}
