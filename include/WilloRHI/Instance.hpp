#pragma once

#include "vulkan/vulkan.h"

#include <functional>

namespace WilloRHI
{
    typedef void* NativeWindowHandle;

    struct InstanceCreateInfo
    {
        NativeWindowHandle windowHandle;
    };

    class Instance
    {
    public:
        VkInstance _instance;
    };

    static Instance CreateInstance(const InstanceCreateInfo& createInfo);
}
