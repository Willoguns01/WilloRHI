#pragma once

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#ifdef __linux__
#if WilloRHI_BUILD_WAYLAND
#define VK_USE_PLATFORM_WAYLAND_KHR
#endif
#if WilloRHI_BUILD_X11
#define VK_USE_PLATFORM_XLIB_KHR
#endif
#endif

namespace WilloRHI
{
    using NativeWindowHandle = void*;
}

#include "WilloRHI/Types.hpp"
#include "WilloRHI/Forward.hpp"

#include "WilloRHI/Sync.hpp"
#include "WilloRHI/Resources.hpp"

#include "WilloRHI/Device.hpp"
#include "WilloRHI/Swapchain.hpp"
#include "WilloRHI/CommandList.hpp"
