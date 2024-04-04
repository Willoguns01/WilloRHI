#pragma once

#include "WilloRHI/WilloRHI.hpp"

#undef CreateSemaphore

namespace WilloRHI
{
    struct SwapchainCreateInfo
    {
        NativeWindowHandle windowHandle = nullptr;
        Format format = Format::UNDEFINED;
        PresentMode presentMode = PresentMode::IMMEDIATE;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t framesInFlight = 2;
    };

    enum class SubmitQueueType {
        GRAPHICS,
        COMPUTE,
        TRANSFER
    };

    struct CommandSubmitInfo
    {
        SubmitQueueType queueType = SubmitQueueType::GRAPHICS;
        std::vector<std::pair<TimelineSemaphore, uint64_t>> waitTimelineSemaphores;
        std::vector<std::pair<TimelineSemaphore, uint64_t>> signalTimelineSemaphores;
        std::vector<BinarySemaphore> waitBinarySemaphores;
        std::vector<BinarySemaphore> signalBinarySemaphores;
        std::vector<CommandList> commandLists;
        bool syncToTimeline = true;
    };

    struct PresentInfo
    {
        std::vector<BinarySemaphore> waitSemaphores = {};
        Swapchain* swapchain;
    };

    // these are some sane-ish defaults to allow any application run just fine
    // this exists for optimisation purposes where the client app may want
    // to reduce memory usage or runtime performance costs of having many descriptors
    struct ResourceCountInfo
    {
        uint64_t bufferCount = Util::Power(2, 20);
        uint64_t imageCount = Util::Power(2, 20);
        uint64_t imageViewCount = Util::Power(2, 20);
        uint64_t samplerCount = Util::Power(2, 20);
    };

    typedef void(*RHILoggingFunc)(const std::string&);
    struct DeviceCreateInfo
    {
        std::string applicationName = "WilloRHI Application";
        bool validationLayers = false;
        RHILoggingFunc logCallback = nullptr;
        bool logInfo = false;
        ResourceCountInfo resourceCounts = {};
        uint32_t maxFramesInFlight = 2;
    };

    class Device
    {
    public:
        Device() = default;

        static Device CreateDevice(const DeviceCreateInfo& createInfo);

        void Cleanup();
        void WaitIdle() const;

        // creation functions

        BinarySemaphore CreateBinarySemaphore();
        TimelineSemaphore CreateTimelineSemaphore(uint64_t initialValue);
        Swapchain CreateSwapchain(const SwapchainCreateInfo& createInfo);

        CommandList GetCommandList();

        BufferId CreateBuffer(const BufferCreateInfo& createInfo);

        // deletion functions

        void DestroyBinarySemaphore(BinarySemaphore semaphore);
        void DestroyTimelineSemaphore(TimelineSemaphore semaphore);
        void DestroySwapchain(Swapchain swapchain);

        // functionality

        void NextFrame();
        uint64_t GetFrameNum();

        void WaitSemaphore(TimelineSemaphore semaphore, uint64_t value, uint64_t timeout);
        uint64_t GetSemaphoreValue(TimelineSemaphore semaphore);

        void QueuePresent(const PresentInfo& presentInfo);
        void QueueSubmit(const CommandSubmitInfo& submitInfo);

        // call at the beginning of each frame - finalises destruction of 
        // any resources set as destroyed for that frame
        void CollectGarbage();

        void LogMessage(const std::string& message, bool error = true);
        void ErrorCheck(uint64_t errorCode);

    protected:
        std::shared_ptr<ImplDevice> impl = nullptr;
    };
}
