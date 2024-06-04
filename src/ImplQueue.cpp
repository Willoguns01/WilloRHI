#include "ImplQueue.hpp"
#include "ImplResources.hpp"

#include "WilloRHI/Sync.hpp"

#include <VkBootstrap.h>

// TODO: can remove once we get rid of VkBootstrap
#include "ImplDevice.hpp"

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
        _vkDevice = static_cast<VkDevice>(device.GetDeviceNativeHandle());

        // TODO: remove once we get rid of VkBootstrap
        vkb::Device vkbDevice = device.impl->_vkbDevice;

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

        _submissionTimeline = WilloRHI::TimelineSemaphore::Create(_device, 0);

        device.LogMessage("Created queue of type " + _queueStr, false);
    }

    ImplQueue::~ImplQueue() {
        Cleanup();
    }

    void ImplQueue::Cleanup() {
        for (auto it : _commandPools) {
            vkDestroyCommandPool(_vkDevice, it.second, nullptr);
        }
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
            VkCommandBufferAllocateInfo allocInfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .pNext = nullptr,
                .commandPool = _commandPools.at(threadId),
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1
            };

            VkCommandBuffer tempHandle = VK_NULL_HANDLE;

            _device.ErrorCheck(vkAllocateCommandBuffers(_vkDevice, &allocInfo, &tempHandle));

            commandList = CommandList(_device, std::this_thread::get_id(), (void*)tempHandle);

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
            CommandList cmdList = submitInfo.commandLists.at(i);
            cmdList.FlushBarriers();
            VkCommandBuffer vkBuf = static_cast<VkCommandBuffer>(cmdList.GetNativeHandle());
            cmdBuffers.push_back(vkBuf);
        }

        std::vector<VkSemaphore> waitSemaphores;
        std::vector<uint64_t> waitValues;
        std::vector<VkPipelineStageFlags> waitStageFlags;
        std::vector<VkSemaphore> signalSemaphores;
        std::vector<uint64_t> signalValues;

        for (int i = 0; i < submitInfo.waitTimelineSemaphores.size(); i++) {
            waitSemaphores.push_back((VkSemaphore)submitInfo.waitTimelineSemaphores[i].first.GetNativeHandle());
            waitValues.push_back(submitInfo.waitTimelineSemaphores[i].second);
            waitStageFlags.push_back(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        }

        for (int i = 0; i < submitInfo.waitBinarySemaphores.size(); i++) {
            waitSemaphores.push_back((VkSemaphore)submitInfo.waitBinarySemaphores[i].GetNativeHandle());
            waitValues.push_back(0);
            waitStageFlags.push_back(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        }

        for (int i = 0; i < submitInfo.signalTimelineSemaphores.size(); i++) {
            signalSemaphores.push_back((VkSemaphore)submitInfo.signalTimelineSemaphores[i].first.GetNativeHandle());
            signalValues.push_back(submitInfo.signalTimelineSemaphores[i].second);
        }

        for (int i = 0; i < submitInfo.signalBinarySemaphores.size(); i++) {
            signalSemaphores.push_back((VkSemaphore)submitInfo.signalBinarySemaphores[i].GetNativeHandle());
            signalValues.push_back(0);
        }

        // submission timeline value
        _timelineValue += 1;
        signalSemaphores.push_back((VkSemaphore)_submissionTimeline.GetNativeHandle());
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
        std::vector<VkSemaphore> waitSemaphores;
        for (int i = 0; i < presentInfo.waitSemaphores.size(); i++) {
            waitSemaphores.push_back((VkSemaphore)presentInfo.waitSemaphores[i].GetNativeHandle());
        }

        Swapchain presentSwapchain = presentInfo.swapchain;

        uint32_t currentIndex = (uint32_t)presentInfo.swapchain.GetCurrentImageIndex();
        VkSwapchainKHR vkSwapchain = (VkSwapchainKHR)presentSwapchain.GetNativeHandle();

        VkPresentInfoKHR info = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext = nullptr,
            .waitSemaphoreCount = (uint32_t)presentInfo.waitSemaphores.size(),
            .pWaitSemaphores = waitSemaphores.data(),
            .swapchainCount = 1,
            .pSwapchains = &vkSwapchain,
            .pImageIndices = &currentIndex,
            .pResults = {}
        };

        VkResult result = vkQueuePresentKHR(_vkQueue, &info);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            presentSwapchain.SetNeedsResize(true);
            _device.LogMessage("Present needs resize", false);
        }
    }

    void Queue::CollectGarbage() { impl->CollectGarbage(); }
    void ImplQueue::CollectGarbage()
    {
        uint64_t gpuTimeline = _submissionTimeline.GetValue();

        while (!_pendingCommandLists.empty()) {
            std::pair<uint64_t, CommandList> cmdPair = _pendingCommandLists.front();

            // if needed value is higher than current timeline value, is in future
            // can safely break loop because any others after this will be the same
            if (cmdPair.first > gpuTimeline)
                break;

            _pendingCommandLists.pop_front();

            CommandList cmdList = cmdPair.second;
            DeviceResources* resources = static_cast<DeviceResources*>(_device.GetDeviceResources());

            _cmdListPool.at(cmdList.GetThreadId()).enqueue(cmdList);

            DeletionQueues* deletionQueues = (DeletionQueues*)cmdList.GetDeletionQueue();

            BufferId bufferHandle{};
            while (deletionQueues->bufferQueue.try_dequeue(bufferHandle)) {
                _device.DestroyBuffer(bufferHandle);
            }

            ImageId imageHandle{};
            while (deletionQueues->imageQueue.try_dequeue(imageHandle)) {
                _device.DestroyImage(imageHandle);
            }

            ImageViewId viewHandle{};
            while (deletionQueues->imageViewQueue.try_dequeue(viewHandle)) {
                _device.DestroyImageView(viewHandle);
            }

            SamplerId samplerHandle{};
            while (deletionQueues->samplerQueue.try_dequeue(samplerHandle)) {
                _device.DestroySampler(samplerHandle);
            }

            vkResetCommandBuffer((VkCommandBuffer)cmdList.GetNativeHandle(), 0);
        }
    }
}
