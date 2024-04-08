#include "ImplSwapchain.hpp"
#include "ImplQueue.hpp"
#include "ImplDevice.hpp"
#include "ImplCommandList.hpp"

#include "ImplSync.hpp"

#include <VkBootstrap.h>

namespace WilloRHI
{
    Queue Queue::Create(Device device, QueueType queueType)
    {
        Queue newQueue;
        newQueue.impl = std::make_shared<ImplQueue>();
        newQueue.impl->Init(device, queueType, newQueue);
        return newQueue;
    }

    void ImplQueue::Init(Device device, QueueType queueType, Queue parent)
    {
        _device = device;
        _vkDevice = device.impl->_vkDevice;
        vkb::Device vkbDevice = device.impl->_vkbDevice;

        device.impl->_deviceQueues.push_back(parent);

        if (queueType == QueueType::GRAPHICS) {
            // this is the graphics queue! whoa!
            // the client should only ever make one of these
            // make sure the device knows about it, for deletion queue purposes
        }

        vkb::QueueType vkbType;

        switch (queueType)
        {
            case QueueType::GRAPHICS:
                vkbType = vkb::QueueType::graphics;
                _queueStr = "GRAPHICS";
                break;
            case QueueType::COMPUTE:
                vkbType = vkb::QueueType::compute;
                _queueStr = "COMPUTE";
                break;
            case QueueType::TRANSFER:
                vkbType = vkb::QueueType::transfer;
                _queueStr = "TRANSFER";
                break;
            default:
                // how, even?
                vkbType = vkb::QueueType::graphics;
                _queueStr = "GRAPHICS";
                break;
        }

        _queueType = queueType;
        _vkQueue = vkbDevice.get_queue(vkbType).value();
        _vkQueueIndex = vkbDevice.get_queue_index(vkbType).value();

        _submissionTimeline = device.CreateTimelineSemaphore(0);

        device.LogMessage("Created queue of type " + _queueStr, false);
    }

    CommandList Queue::GetCmdList() { return impl->GetCmdList(); }
    CommandList ImplQueue::GetCmdList()
    {
        std::thread::id threadId = std::this_thread::get_id();
        PoolMapType* map = &_cmdListPool;

        // create the pool if it doesn't exist
        if (_commandPools.find(threadId) == _commandPools.end()) {
            VkCommandPool newPool = VK_NULL_HANDLE;

            VkCommandPoolCreateInfo poolInfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .pNext = nullptr,
                .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = _vkQueueIndex
            };

            _device.ErrorCheck(vkCreateCommandPool(_vkDevice, &poolInfo, nullptr, &newPool));

            _commandPools.insert({ threadId, newPool });

            _cmdListPool.insert({ threadId, moodycamel::ConcurrentQueue<CommandList>()});
        
            _device.LogMessage("Created new CommandPool for queue of type " + _queueStr + " on thread " + std::to_string(std::hash<std::thread::id>()(threadId)), false);
        }

        CommandList commandList;
        if (!map->at(threadId).try_dequeue(commandList)) {
            commandList.impl = std::make_shared<ImplCommandList>();

            VkCommandBufferAllocateInfo allocInfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .pNext = nullptr,
                .commandPool = _commandPools.at(threadId),
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1
            };

            _device.ErrorCheck(vkAllocateCommandBuffers(_vkDevice, &allocInfo, &commandList.impl->_vkCommandBuffer));
        
            commandList.impl->_device = _device;
            commandList.impl->_threadId = std::this_thread::get_id();

            _device.LogMessage("Allocated CommandList for thread " + std::to_string(std::hash<std::thread::id>()(threadId)), false);
        }

        return commandList;
    }

    void Queue::Submit(const CommandSubmitInfo& submitInfo) { impl->Submit(submitInfo); }
    void ImplQueue::Submit(const CommandSubmitInfo& submitInfo)
    {
        int64_t commandListCount = submitInfo.commandLists.size();
        std::vector<VkCommandBuffer> cmdBuffers;

        for (int i = 0; i < commandListCount; i++) {
            cmdBuffers.push_back(submitInfo.commandLists.at(i).impl->_vkCommandBuffer);
        }

        std::vector<VkSemaphore> waitSemaphores;
        std::vector<uint64_t> waitValues;
        std::vector<VkPipelineStageFlags> waitStageFlags;
        std::vector<VkSemaphore> signalSemaphores;
        std::vector<uint64_t> signalValues;

        for (int i = 0; i < submitInfo.waitTimelineSemaphores.size(); i++) {
            waitSemaphores.push_back(submitInfo.waitTimelineSemaphores[i].first.impl->vkSemaphore);
            waitValues.push_back(submitInfo.waitTimelineSemaphores[i].second);
            waitStageFlags.push_back(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        }

        for (int i = 0; i < submitInfo.waitBinarySemaphores.size(); i++) {
            waitSemaphores.push_back(submitInfo.waitBinarySemaphores[i].impl->vkSemaphore);
            waitValues.push_back(0);
            waitStageFlags.push_back(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        }

        for (int i = 0; i < submitInfo.signalTimelineSemaphores.size(); i++) {
            signalSemaphores.push_back(submitInfo.signalTimelineSemaphores[i].first.impl->vkSemaphore);
            signalValues.push_back(submitInfo.signalTimelineSemaphores[i].second);
        }

        for (int i = 0; i < submitInfo.signalBinarySemaphores.size(); i++) {
            signalSemaphores.push_back(submitInfo.signalBinarySemaphores[i].impl->vkSemaphore);
            signalValues.push_back(0);
        }

        // submission timeline value
        _timelineValue += 1;
        signalSemaphores.push_back(_submissionTimeline.impl->vkSemaphore);
        signalValues.push_back(_timelineValue);

        // for garbage collection later
        for (int i = 0; i < commandListCount; i++) {
            _pendingCommandLists.push_back(std::pair(_timelineValue, submitInfo.commandLists[i]));
        }

        VkTimelineSemaphoreSubmitInfo timelineSubmitInfo = {
            .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
            .pNext = nullptr,
            .waitSemaphoreValueCount = (uint32_t)waitValues.size(),
            .pWaitSemaphoreValues = waitValues.data(),
            .signalSemaphoreValueCount = (uint32_t)signalValues.size(),
            .pSignalSemaphoreValues = signalValues.data()
        };

        VkSubmitInfo info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = &timelineSubmitInfo,
            .waitSemaphoreCount = (uint32_t)waitSemaphores.size(),
            .pWaitSemaphores = waitSemaphores.data(),
            .pWaitDstStageMask = waitStageFlags.data(),
            .commandBufferCount = (uint32_t)commandListCount,
            .pCommandBuffers = cmdBuffers.data(),
            .signalSemaphoreCount = (uint32_t)signalSemaphores.size(),
            .pSignalSemaphores = signalSemaphores.data()
        };

        _device.ErrorCheck(vkQueueSubmit(_vkQueue, 1, &info, VK_NULL_HANDLE));
    }

    void Queue::Present(const PresentInfo& presentInfo) { impl->Present(presentInfo); }
    void ImplQueue::Present(const PresentInfo& presentInfo)
    {
        if (_queueType != QueueType::GRAPHICS) {
            // what the fuck are you even trying to accomplish?
            _device.LogMessage("Only a graphics queue can be used for presentation");
            return;
        }

        std::vector<VkSemaphore> waitSemaphores;
        for (int i = 0; i < presentInfo.waitSemaphores.size(); i++) {
            waitSemaphores.push_back(presentInfo.waitSemaphores[i].impl->vkSemaphore);
        }

        uint32_t currentIndex = (uint32_t)presentInfo.swapchain.GetCurrentImageIndex();

        VkPresentInfoKHR info = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext = nullptr,
            .waitSemaphoreCount = (uint32_t)presentInfo.waitSemaphores.size(),
            .pWaitSemaphores = waitSemaphores.data(),
            .swapchainCount = 1,
            .pSwapchains = &presentInfo.swapchain.impl->_vkSwapchain,
            .pImageIndices = &currentIndex,
            .pResults = {}
        };

        VkResult result = vkQueuePresentKHR(_vkQueue, &info);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            presentInfo.swapchain.impl->_needResize = true;
            _device.LogMessage("Present needs resize", false);
        }
    }

    void Queue::CollectGarbage() { impl->CollectGarbage(); }
    void ImplQueue::CollectGarbage()
    {
        uint64_t gpuTimeline = _device.GetSemaphoreValue(_submissionTimeline);

        while (!_pendingCommandLists.empty()) {
            std::pair<uint64_t, CommandList> cmdPair = _pendingCommandLists.front();

            // if needed value is higher than current timeline value, is in future
            // can safely break loop because any others after this will be the same
            if (cmdPair.first > gpuTimeline)
                break;

            _pendingCommandLists.pop_front();

            CommandList cmdList = cmdPair.second;
            DeviceResources* resources = &_device.impl->_resources;

            _cmdListPool.at(cmdList.impl->_threadId).enqueue(cmdList);

            // image
            ImageId imageHandle{};
            while (cmdList.impl->_deletionQueues.imageQueue.try_dequeue(imageHandle)) {
                ImageResource& rsrc = resources->images.At(imageHandle.id);
                vmaDestroyImage(_device.impl->_allocator, rsrc.image, rsrc.allocation);
                resources->images.freeSlotQueue.enqueue(imageHandle.id);
            }

            // image view
            ImageViewId viewHandle{};
            while (cmdList.impl->_deletionQueues.imageViewQueue.try_dequeue(viewHandle)) {
                ImageViewResource& rsrc = resources->imageViews.At(viewHandle.id);
                vkDestroyImageView(_vkDevice, rsrc.imageView, nullptr);
                resources->imageViews.freeSlotQueue.enqueue(viewHandle.id);
            }

            // sampler
            SamplerId samplerHandle{};
            while (cmdList.impl->_deletionQueues.samplerQueue.try_dequeue(samplerHandle)) {
                SamplerResource& rsrc = resources->samplers.At(samplerHandle.id);
                vkDestroySampler(_vkDevice, rsrc.sampler, nullptr);
                resources->samplers.freeSlotQueue.enqueue(samplerHandle.id);
            }

            BufferId bufferHandle{};
            while (cmdList.impl->_deletionQueues.bufferQueue.try_dequeue(bufferHandle)) {
                BufferResource& rsrc = resources->buffers.At(bufferHandle.id);
                vmaDestroyBuffer(_device.impl->_allocator, rsrc.buffer, rsrc.allocation);
                resources->buffers.freeSlotQueue.enqueue(bufferHandle.id);
            }

            vkResetCommandBuffer(cmdList.impl->_vkCommandBuffer, 0);
        }
    }
}
