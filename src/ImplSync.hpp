#pragma once

#include "WilloRHI/Sync.hpp"

namespace WilloRHI
{
    struct ImplBinarySemaphore {
        VkSemaphore vkSemaphore = VK_NULL_HANDLE;
    };

    struct ImplTimelineSemaphore {
        VkSemaphore vkSemaphore = VK_NULL_HANDLE;
    };
}
