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

    void CommandList::ClearImage(ImageId image, const float clearColour[4], const ImageSubresourceRange& subresourceRange) {
        impl->ClearImage(image, clearColour, subresourceRange); }
    void ImplCommandList::ClearImage(ImageId image, const float clearColour[4], const ImageSubresourceRange& subresourceRange)
    {
        VkClearColorValue vkClear = { {clearColour[0], clearColour[1], clearColour[2], clearColour[3]} };

        VkImageSubresourceRange resourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = subresourceRange.baseLevel,
            .levelCount = subresourceRange.numLevels,
            .baseArrayLayer = subresourceRange.baseLayer,
            .layerCount = subresourceRange.numLayers
        };

        VkImage vkImage = static_cast<VkImage>(_device.GetImageNativeHandle(image));
        vkCmdClearColorImage(_vkCommandBuffer, vkImage, VK_IMAGE_LAYOUT_GENERAL, &vkClear, 1, &resourceRange);
    }

    void CommandList::TransitionImageLayout(ImageId image, const ImageMemoryBarrierInfo& barrierInfo) {
        impl->TransitionImageLayout(image, barrierInfo); }
    // TODO: we should "queue" barriers,
    // then once we run some other kind of cmd,
    // we flush them all with a single call
    void ImplCommandList::TransitionImageLayout(ImageId image, const ImageMemoryBarrierInfo& barrierInfo)
    {
        VkImageAspectFlags aspectMask = (static_cast<VkImageLayout>(barrierInfo.dstLayout) == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        VkImageSubresourceRange resourceRange = {
            .aspectMask = aspectMask,
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
            .pImageMemoryBarriers = &imageBarrier
        };

        vkCmdPipelineBarrier2(_vkCommandBuffer, &depInfo);

        imageResource.currentLayout = static_cast<VkImageLayout>(barrierInfo.dstLayout);
        imageResource.currentAccessFlags = static_cast<VkAccessFlagBits2>(barrierInfo.dstAccess);
        imageResource.currentPipelineStage = static_cast<VkPipelineStageFlagBits2>(barrierInfo.dstStage);
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

        VkImageAspectFlags srcAspect = (static_cast<VkImageLayout>(srcResource.currentLayout) == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        VkImageAspectFlags dstAspect = (static_cast<VkImageLayout>(dstResource.currentLayout) == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        
        for (uint32_t i = 0; i < numRegions; i++) {
            vkRegions[i] = {
                .srcSubresource = {
                    .aspectMask = srcAspect,
                    .mipLevel = regions[i].srcSubresource.level,
                    .baseArrayLayer = regions[i].srcSubresource.baseLayer,
                    .layerCount = regions[i].srcSubresource.numLayers
                },
                .srcOffset = {regions[i].srcOffset.x,regions[i].srcOffset.y,regions[i].srcOffset.z},
                .dstSubresource = {
                    .aspectMask = dstAspect,
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

    void CommandList::CopyBufferToImage(BufferId srcBuffer, ImageId dstImage, uint32_t numRegions, BufferImageCopyRegion* regions) {
        impl->CopyBufferToImage(srcBuffer, dstImage, numRegions, regions); }
    void ImplCommandList::CopyBufferToImage(BufferId srcBuffer, ImageId dstImage, uint32_t numRegions, BufferImageCopyRegion* regions) 
    {
        BufferResource& srcResource = _resources->buffers.At(srcBuffer);
        ImageResource& dstResource = _resources->images.At(dstImage);

        std::vector<VkBufferImageCopy> vkRegions;
        vkRegions.resize(numRegions);

        VkImageAspectFlags dstAspect = (static_cast<VkImageLayout>(dstResource.currentLayout) == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

        for (uint32_t i = 0; i < numRegions; i++) {
            vkRegions[i] = {
                .bufferOffset = regions[i].bufferOffset,
                .bufferRowLength = regions[i].rowLength,
                .bufferImageHeight = regions[i].imageHeight,
                .imageSubresource = {
                    .aspectMask = dstAspect,
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
