#include "ImplCommandList.hpp"
#include "ImplDevice.hpp"

namespace WilloRHI
{
    void CommandList::Reset() { impl->Reset(); }
    void ImplCommandList::Reset()
    {
        vkResetCommandBuffer(_vkCommandBuffer, 0);
    }

    void CommandList::Begin() { impl->Begin(); }
    void ImplCommandList::Begin()
    {
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

        VkImage vkImage = _device->_resources.images.resources[image.id].image;
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

        VkImage vkImage = _device->_resources.images.resources[image.id].image;

        VkImageMemoryBarrier2 imageBarrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .pNext = nullptr,
            .srcStageMask = static_cast<VkPipelineStageFlagBits2>(barrierInfo.srcStage),
            .srcAccessMask = static_cast<VkAccessFlagBits2>(barrierInfo.srcAccess),
            .dstStageMask = static_cast<VkPipelineStageFlagBits2>(barrierInfo.dstStage),
            .dstAccessMask = static_cast<VkAccessFlagBits2>(barrierInfo.dstAccess),
            .oldLayout = static_cast<VkImageLayout>(barrierInfo.srcLayout),
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
    }
}
