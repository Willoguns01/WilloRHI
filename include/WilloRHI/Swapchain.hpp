#pragma once

#include "WilloRHI/Types.hpp"
#include "WilloRHI/Device.hpp"
#include "WilloRHI/Sync.hpp"

#include <stdint.h>

namespace WilloRHI
{
    using NativeWindowHandle = void*;

    struct SwapchainCreateInfo
    {
        NativeWindowHandle windowHandle = nullptr;
        Format format = Format::UNDEFINED;
        PresentMode presentMode = PresentMode::IMMEDIATE;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t framesInFlight = 2;
    };

    class Swapchain
    {
    public:
        static Swapchain Create(Device device, const SwapchainCreateInfo& createInfo);

        Swapchain() = default;

        ImageId AcquireNextImage();
        uint64_t GetCurrentImageIndex() const;

        BinarySemaphore const& GetAcquireSemaphore();

        bool NeedsResize() const;
        void Resize(uint32_t width, uint32_t height, PresentMode presentMode = PresentMode::FIFO);

    protected:
        friend ImplDevice;
        friend ImplQueue;
        std::shared_ptr<ImplSwapchain> impl = nullptr;

    };
}
