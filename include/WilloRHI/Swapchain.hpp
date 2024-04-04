#pragma once

#include "WilloRHI/WilloRHI.hpp"

namespace WilloRHI
{
    class Swapchain
    {
    public:
        Swapchain() = default;

        ImageId AcquireNextImage();
        uint64_t GetCurrentImageIndex() const;

        BinarySemaphore const& GetAcquireSemaphore();

        bool NeedsResize() const;
        void Resize(uint32_t width, uint32_t height, PresentMode presentMode = PresentMode::FIFO);

    protected:
        friend ImplDevice;
        std::shared_ptr<ImplSwapchain> impl = nullptr;

    };
}
