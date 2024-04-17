#pragma once

#include "WilloRHI/Swapchain.hpp"
#include "WilloRHI/CommandList.hpp"

#include <vector>

namespace WilloRHI
{
    struct CommandSubmitInfo
    {
        std::vector<std::pair<TimelineSemaphore, uint64_t>> waitTimelineSemaphores;
        std::vector<std::pair<TimelineSemaphore, uint64_t>> signalTimelineSemaphores;
        std::vector<BinarySemaphore> waitBinarySemaphores;
        std::vector<BinarySemaphore> signalBinarySemaphores;
        std::vector<CommandList> commandLists;
    };

    struct PresentInfo
    {
        std::vector<BinarySemaphore> waitSemaphores = {};
        WilloRHI::Swapchain swapchain;
    };

    enum class QueueType {
        GRAPHICS,
        COMPUTE,
        TRANSFER
    };

    class Queue
    {
    public:
        Queue() = default;
        static Queue Create(Device device, QueueType queueType);

        CommandList GetCmdList();

        void Submit(const CommandSubmitInfo& submitInfo);
        void Present(const PresentInfo& presentInfo);

        // call periodically - preferrably once per frame - to clear out unused resources
        // also needs to be called before exit to prevent validation errors
        void CollectGarbage();

    protected:
        friend ImplDevice;

        std::shared_ptr<ImplQueue> impl = nullptr;
    };
}
