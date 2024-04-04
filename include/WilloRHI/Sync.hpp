#pragma once

#include "WilloRHI/WilloRHI.hpp"

namespace WilloRHI
{
    struct BinarySemaphore {
    public:
        BinarySemaphore() = default;

        //void* GetNativeHandle();

    protected:
        friend ImplDevice;
        friend ImplSwapchain;
        std::shared_ptr<ImplBinarySemaphore> impl = nullptr;
    };

    struct TimelineSemaphore {
    public:
        TimelineSemaphore() = default;

        //void* GetNativeHandle();

    protected:
        friend ImplDevice;
        friend ImplSwapchain;
        std::shared_ptr<ImplTimelineSemaphore> impl = nullptr;
    };
}
