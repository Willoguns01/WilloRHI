#pragma once

#include "WilloRHI/WilloRHI.hpp"

namespace WilloRHI
{
    class Swapchain
    {
    public:
        Swapchain();

        ImageId AcquireNextImage();
        uint64_t GetCurrentImageIndex() const;

        BinarySemaphore const& GetAcquireSemaphore();

    protected:
        friend ImplDevice;

        std::shared_ptr<ImplSwapchain> impl;

    };
}
