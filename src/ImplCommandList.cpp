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
        FlushBarriers();
        _device.UnlockResources_Shared();
        vkEndCommandBuffer(_vkCommandBuffer);
    }

    void CommandList::BeginRendering(const RenderPassBeginInfo& beginInfo) {
        impl->BeginRendering(beginInfo); }
    void ImplCommandList::BeginRendering(const RenderPassBeginInfo& beginInfo)
    {
        FlushBarriers();

        std::vector<VkRenderingAttachmentInfo> vkColourAttachments;
        vkColourAttachments.reserve(beginInfo.colourAttachments.size());
        for (const auto& attachment : beginInfo.colourAttachments) {
            vkColourAttachments.push_back({
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .pNext = nullptr,
                .imageView = _resources->imageViews.At(attachment.imageView).imageView,
                .imageLayout = static_cast<VkImageLayout>(attachment.imageLayout),
                .resolveMode = VK_RESOLVE_MODE_NONE,
                .resolveImageView = VK_NULL_HANDLE,
                .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .loadOp = static_cast<VkAttachmentLoadOp>(attachment.loadOp),
                .storeOp = static_cast<VkAttachmentStoreOp>(attachment.storeOp),
                .clearValue = { .color = { attachment.clearColour.r, attachment.clearColour.g, attachment.clearColour.b, attachment.clearColour.a, } }
            });
        }

        VkRenderingAttachmentInfo vkDepthAttachment = {};
        bool hasDepthAttachment = false;
        if (beginInfo.depthAttachment.has_value()) {
            hasDepthAttachment = true;
            const RenderPassAttachmentInfo& depthInfo = beginInfo.depthAttachment.value();
            vkDepthAttachment = {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .pNext = nullptr,
                .imageView = _resources->imageViews.At(depthInfo.imageView).imageView,
                .imageLayout = static_cast<VkImageLayout>(depthInfo.imageLayout),
                .resolveMode = VK_RESOLVE_MODE_NONE,
                .resolveImageView = VK_NULL_HANDLE,
                .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .loadOp = static_cast<VkAttachmentLoadOp>(depthInfo.loadOp),
                .storeOp = static_cast<VkAttachmentStoreOp>(depthInfo.storeOp),
                .clearValue = { .depthStencil = { .depth = depthInfo.clearColour.r }}
            };
        }

        VkRenderingInfo renderingInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .pNext = nullptr,
            .flags = 0,
            .renderArea = VkRect2D {
                .offset = {.x = beginInfo.renderingArea.offset.x, .y = beginInfo.renderingArea.offset.y},
                .extent = {.width = beginInfo.renderingArea.extent.width, .height = beginInfo.renderingArea.extent.height}
            },
            .layerCount = 1,
            .viewMask = {},
            .colorAttachmentCount = (uint32_t)beginInfo.colourAttachments.size(),
            .pColorAttachments = vkColourAttachments.data(),
            .pDepthAttachment = hasDepthAttachment ? &vkDepthAttachment : nullptr,
            .pStencilAttachment = nullptr
        };

        vkCmdBeginRendering(_vkCommandBuffer, &renderingInfo);

        VkViewport vkViewport = {
            .x = (float)beginInfo.renderingArea.offset.x,
            .y = (float)beginInfo.renderingArea.offset.y,
            .width = (float)beginInfo.renderingArea.extent.width,
            .height = (float)beginInfo.renderingArea.extent.height,
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };
        vkCmdSetViewport(_vkCommandBuffer, 0, 1, &vkViewport);

        vkCmdSetScissor(_vkCommandBuffer, 0, 1, reinterpret_cast<const VkRect2D*>(&beginInfo.renderingArea));
    }

    void CommandList::EndRendering() { impl->EndRendering(); }
    void ImplCommandList::EndRendering()
    {
        vkCmdEndRendering(_vkCommandBuffer);
    }

    void CommandList::PushConstants(uint32_t offset, uint32_t size, void* data) {
        impl->PushConstants(offset, size, data); }
    void ImplCommandList::PushConstants(uint32_t offset, uint32_t size, void* data) {
        FlushBarriers();
        vkCmdPushConstants(_vkCommandBuffer, _currentPipelineLayout, _currentStageFlags, offset, size, data);
    }

    void CommandList::GlobalMemoryBarrier(const GlobalMemoryBarrierInfo& barrierInfo) {
        impl->GlobalMemoryBarrier(barrierInfo); }
    void ImplCommandList::GlobalMemoryBarrier(const GlobalMemoryBarrierInfo& barrierInfo)
    {
        VkMemoryBarrier2 barrier = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
            .pNext = nullptr,
            .srcStageMask = static_cast<VkPipelineStageFlags2>(barrierInfo.srcStage),
            .srcAccessMask = static_cast<VkAccessFlags2>(barrierInfo.srcAccess),
            .dstStageMask = static_cast<VkPipelineStageFlags2>(barrierInfo.dstStage),
            .dstAccessMask = static_cast<VkAccessFlags2>(barrierInfo.dstAccess)
        };

        _globalBarriers.push_back(barrier);
    }

    void CommandList::ImageMemoryBarrier(ImageId image, const ImageMemoryBarrierInfo& barrierInfo) {
        impl->ImageMemoryBarrier(image, barrierInfo); }
    void ImplCommandList::ImageMemoryBarrier(ImageId image, const ImageMemoryBarrierInfo& barrierInfo)
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
            .srcStageMask = static_cast<VkPipelineStageFlags2>(imageResource.currentPipelineStage),
            .srcAccessMask = static_cast<VkAccessFlags2>(imageResource.currentAccessFlags),
            .dstStageMask = static_cast<VkPipelineStageFlags2>(barrierInfo.dstStage),
            .dstAccessMask = static_cast<VkAccessFlags2>(barrierInfo.dstAccess),
            .oldLayout = static_cast<VkImageLayout>(imageResource.currentLayout),
            .newLayout = static_cast<VkImageLayout>(barrierInfo.dstLayout),
            .image = vkImage,
            .subresourceRange = resourceRange
        };

        imageResource.currentLayout = static_cast<VkImageLayout>(barrierInfo.dstLayout);
        imageResource.currentAccessFlags = static_cast<VkAccessFlags2>(barrierInfo.dstAccess);
        imageResource.currentPipelineStage = static_cast<VkPipelineStageFlags2>(barrierInfo.dstStage);
    
        _imageBarriers.push_back(imageBarrier);
    }

    void CommandList::BufferMemoryBarrier(BufferId buffer, const BufferMemoryBarrierInfo& barrierInfo) {
        impl->BufferMemoryBarrier(buffer, barrierInfo); }
    void ImplCommandList::BufferMemoryBarrier(BufferId buffer, const BufferMemoryBarrierInfo& barrierInfo)
    {
        BufferResource& bufferResource = _resources->buffers.At(buffer);

        VkBufferMemoryBarrier2 bufferBarrier = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
            .pNext = nullptr,
            .srcStageMask = bufferResource.currentPipelineStage,
            .srcAccessMask = bufferResource.currentAccessFlags,
            .dstStageMask = static_cast<VkPipelineStageFlags2>(barrierInfo.dstStage),
            .dstAccessMask = static_cast<VkAccessFlags2>(barrierInfo.dstAccess),
            .buffer = bufferResource.buffer,
            .offset = 0,
            .size = bufferResource.createInfo.size
        };

        bufferResource.currentPipelineStage = static_cast<VkPipelineStageFlags2>(barrierInfo.dstStage);
        bufferResource.currentAccessFlags = static_cast<VkAccessFlags2>(barrierInfo.dstAccess);

        _bufferBarriers.push_back(bufferBarrier);
    }

    void CommandList::FlushBarriers() { impl->FlushBarriers(); }
    void ImplCommandList::FlushBarriers()
    {
        if (_globalBarriers.size() == 0 && _bufferBarriers.size() == 0 && _imageBarriers.size() == 0) {
            return;
        }

        VkDependencyInfo dependency = {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext = nullptr,
            .dependencyFlags = 0,
            .memoryBarrierCount = (uint32_t)_globalBarriers.size(),
            .pMemoryBarriers = _globalBarriers.data(),
            .bufferMemoryBarrierCount = (uint32_t)_bufferBarriers.size(),
            .pBufferMemoryBarriers = _bufferBarriers.data(),
            .imageMemoryBarrierCount = (uint32_t)_imageBarriers.size(),
            .pImageMemoryBarriers = _imageBarriers.data()
        };

        vkCmdPipelineBarrier2(_vkCommandBuffer, &dependency);

        _globalBarriers.clear();
        _bufferBarriers.clear();
        _imageBarriers.clear();
    }

    void CommandList::BindComputePipeline(ComputePipeline pipeline) {
        impl->BindComputePipeline(pipeline); }
    void ImplCommandList::BindComputePipeline(ComputePipeline pipeline) {
        FlushBarriers();
        _currentPipeline = VK_PIPELINE_BIND_POINT_COMPUTE;
        _currentPipelineLayout = static_cast<VkPipelineLayout>(pipeline.GetPipelineLayout());
        _currentStageFlags = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
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
        FlushBarriers();
        _currentPipeline = VK_PIPELINE_BIND_POINT_GRAPHICS;
        _currentPipelineLayout = static_cast<VkPipelineLayout>(pipeline.GetPipelineLayout());
        _currentStageFlags = static_cast<VkPipelineStageFlags>(pipeline.GetStageFlags());
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

    void CommandList::BindVertexBuffer(BufferId buffer, uint32_t binding) {
        impl->BindVertexBuffer(buffer, binding); }
    void ImplCommandList::BindVertexBuffer(BufferId buffer, uint32_t binding)
    {
        constexpr VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(_vkCommandBuffer, binding, 1, &_resources->buffers.At(buffer).buffer, &offset);
    }

    void CommandList::BindIndexBuffer(BufferId buffer, uint64_t bufferOffset, IndexType indexType) {
        impl->BindIndexBuffer(buffer, bufferOffset, indexType); }
    void ImplCommandList::BindIndexBuffer(BufferId buffer, uint64_t bufferOffset, IndexType indexType)
    {
        vkCmdBindIndexBuffer(_vkCommandBuffer, _resources->buffers.At(buffer).buffer, bufferOffset, static_cast<VkIndexType>(indexType));
    }

    void CommandList::SetViewport(Viewport viewport) {
        impl->SetViewport(viewport); }
    void ImplCommandList::SetViewport(Viewport viewport)
    {
        VkViewport vkViewport = {
            .x = viewport.x,
            .y = viewport.y,
            .width = viewport.width,
            .height = viewport.height,
            .minDepth = viewport.minDepth,
            .maxDepth = viewport.maxDepth
        };

        // TODO: which of these is faster?

        vkCmdSetViewport(_vkCommandBuffer, 0, 1, &vkViewport);

        //vkCmdSetViewport(_vkCommandBuffer, 0, 1, reinterpret_cast<VkViewport*>(&viewport));
    }

    void CommandList::SetScissor(std::vector<Rect2D> scissor) {
        impl->SetScissor(scissor); }
    void ImplCommandList::SetScissor(std::vector<Rect2D> scissor)
    {
        std::vector<VkRect2D> vkRects;
        vkRects.reserve(scissor.size());
        for (const auto& elem : scissor) {
            vkRects.push_back(VkRect2D{
                .offset = {
                    .x = elem.offset.x,
                    .y = elem.offset.y
                },
                .extent = {
                    .width = elem.extent.width,
                    .height = elem.extent.height
                }
            });
        }

        vkCmdSetScissor(_vkCommandBuffer, 0, (uint32_t)vkRects.size(), vkRects.data());
    }

    void CommandList::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
        impl->Dispatch(groupCountX, groupCountY, groupCountZ); }
    void ImplCommandList::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
    {
        vkCmdDispatch(_vkCommandBuffer, groupCountX, groupCountY, groupCountZ);
    }

    void CommandList::ClearImage(ImageId image, ClearColour clearColour, const ImageSubresourceRange& subresourceRange) {
        impl->ClearImage(image, clearColour, subresourceRange); }
    void ImplCommandList::ClearImage(ImageId image, ClearColour clearColour, const ImageSubresourceRange& subresourceRange)
    {
        FlushBarriers();
        VkClearColorValue vkClear = { {clearColour.r, clearColour.g, clearColour.b, clearColour.a} };

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

    void CommandList::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
        impl->Draw(vertexCount, instanceCount, firstVertex, firstInstance); }
    void ImplCommandList::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
    {
        vkCmdDraw(_vkCommandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
    }

    void CommandList::DrawIndirect(BufferId argBuffer, uint64_t offset, uint32_t drawCount) {
        impl->DrawIndirect(argBuffer, offset, drawCount); }
    void ImplCommandList::DrawIndirect(BufferId argBuffer, uint64_t offset, uint32_t drawCount)
    {
        vkCmdDrawIndirect(_vkCommandBuffer, _resources->buffers.At(argBuffer).buffer, offset, drawCount, sizeof(DrawIndirectCommand));
    }

    void CommandList::DrawIndirectCount(BufferId argBuffer, uint64_t offset, BufferId countBuffer, uint64_t countBufferOffset, uint32_t maxDrawCount) {
        impl->DrawIndirectCount(argBuffer, offset, countBuffer, countBufferOffset, maxDrawCount); }
    void ImplCommandList::DrawIndirectCount(BufferId argBuffer, uint64_t offset, BufferId countBuffer, uint64_t countBufferOffset, uint32_t maxDrawCount)
    {
        vkCmdDrawIndirectCount(_vkCommandBuffer, _resources->buffers.At(argBuffer).buffer, offset, _resources->buffers.At(countBuffer).buffer, countBufferOffset, maxDrawCount, sizeof(DrawIndirectCommand));
    }

    void CommandList::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t vertexOffset, uint32_t firstInstance) {
        impl->DrawIndexed(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance); }
    void ImplCommandList::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t vertexOffset, uint32_t firstInstance)
    {
        vkCmdDrawIndexed(_vkCommandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
    }

    void CommandList::DrawIndexedIndirect(BufferId argBuffer, uint64_t offset, uint32_t drawCount) {
        impl->DrawIndexedIndirect(argBuffer, offset, drawCount); }
    void ImplCommandList::DrawIndexedIndirect(BufferId argBuffer, uint64_t offset, uint32_t drawCount)
    {
        vkCmdDrawIndexedIndirect(_vkCommandBuffer, _resources->buffers.At(argBuffer).buffer, offset, drawCount, sizeof(DrawIndexedIndirectCommand));
    }

    void CommandList::DrawIndexedIndirectCount(BufferId argBuffer, uint64_t offset, BufferId countBuffer, uint64_t countBufferOffset, uint32_t maxDrawCount) {
        impl->DrawIndexedIndirectCount(argBuffer, offset, countBuffer, countBufferOffset, maxDrawCount); }
    void ImplCommandList::DrawIndexedIndirectCount(BufferId argBuffer, uint64_t offset, BufferId countBuffer, uint64_t countBufferOffset, uint32_t maxDrawCount)
    {
        vkCmdDrawIndexedIndirectCount(_vkCommandBuffer, _resources->buffers.At(argBuffer).buffer, offset, _resources->buffers.At(countBuffer).buffer, countBufferOffset, maxDrawCount, sizeof(DrawIndexedIndirectCommand));
    }

    void CommandList::CopyImage(ImageId srcImage, ImageId dstImage, uint32_t numRegions, ImageCopyRegion* regions) {
        impl->CopyImage(srcImage, dstImage, numRegions, regions); }
    void ImplCommandList::CopyImage(ImageId srcImage, ImageId dstImage, uint32_t numRegions, ImageCopyRegion* regions) 
    {
        FlushBarriers();
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
        FlushBarriers();
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
        FlushBarriers();
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
        FlushBarriers();
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
