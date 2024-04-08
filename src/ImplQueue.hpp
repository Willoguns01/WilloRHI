#pragma once

#include <deque>
#include <unordered_map>
#include "concurrentqueue.h"

#include "WilloRHI/Queue.hpp"
#include "WilloRHI/CommandList.hpp"

#include <vulkan/vulkan.h>

namespace WilloRHI
{
    struct ImplQueue
    {
        Device _device;
        VkDevice _vkDevice = VK_NULL_HANDLE;
        QueueType _queueType;
        VkQueue _vkQueue = VK_NULL_HANDLE;
        uint32_t _vkQueueIndex = 0;
        std::string _queueStr;

        std::unordered_map<std::thread::id, VkCommandPool> _commandPools;
        typedef std::unordered_map<std::thread::id, moodycamel::ConcurrentQueue<CommandList>> PoolMapType;
        PoolMapType _cmdListPool;

        std::deque<std::pair<uint64_t, CommandList>> _pendingCommandLists;
        TimelineSemaphore _submissionTimeline;
        uint64_t _timelineValue = 0;

        void Init(Device device, QueueType queueType, Queue parent);

        CommandList GetCmdList();

        void Submit(const CommandSubmitInfo& submitInfo);
        void Present(const PresentInfo& presentInfo);

        void CollectGarbage();
    };
}
