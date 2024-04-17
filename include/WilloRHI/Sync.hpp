#pragma once

#include "WilloRHI/Forward.hpp"

#include <memory>

namespace WilloRHI
{
    struct BinarySemaphore {
    public:
        BinarySemaphore() = default;
        static BinarySemaphore Create(Device device);

        void* GetNativeHandle() const;

    protected:
        friend ImplDevice;
        friend ImplSwapchain;
        friend ImplQueue;
        std::shared_ptr<ImplBinarySemaphore> impl = nullptr;
    };

    struct TimelineSemaphore {
    public:
        TimelineSemaphore() = default;
        static TimelineSemaphore Create(Device device, uint64_t value);

        void* GetNativeHandle() const;

        void WaitValue(uint64_t value, uint64_t timeout);
        uint64_t GetValue();

    protected:
        friend ImplDevice;
        friend ImplSwapchain;
        friend ImplQueue;
        std::shared_ptr<ImplTimelineSemaphore> impl = nullptr;
    };
}
