#pragma once

#include "WilloRHI/Device.hpp"
#include "WilloRHI/Sync.hpp"

#include <vulkan/vulkan.h>

namespace WilloRHI
{
    struct ImplBinarySemaphore {
        void Init(Device device);
        ~ImplBinarySemaphore();

        Device m_Device;
        VkDevice m_vkDevice = VK_NULL_HANDLE;

        VkSemaphore vkSemaphore = VK_NULL_HANDLE;
    };

    struct ImplTimelineSemaphore {
        void Init(Device device, uint64_t value);
        ~ImplTimelineSemaphore();

        void WaitValue(uint64_t value, uint64_t timeout);
        uint64_t GetValue();

        Device m_Device;
        VkDevice m_vkDevice = VK_NULL_HANDLE;

        VkSemaphore vkSemaphore = VK_NULL_HANDLE;
    };
}
