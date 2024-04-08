#pragma once

#include "WilloRHI/Forward.hpp"

#include <memory>

namespace WilloRHI
{
    struct BinarySemaphore {
    public:
        BinarySemaphore() = default;

        //void* GetNativeHandle();

    protected:
        friend ImplDevice;
        friend ImplSwapchain;
        friend ImplQueue;
        std::shared_ptr<ImplBinarySemaphore> impl = nullptr;
    };

    struct TimelineSemaphore {
    public:
        TimelineSemaphore() = default;

        //void* GetNativeHandle();

    protected:
        friend ImplDevice;
        friend ImplSwapchain;
        friend ImplQueue;
        std::shared_ptr<ImplTimelineSemaphore> impl = nullptr;
    };
}
