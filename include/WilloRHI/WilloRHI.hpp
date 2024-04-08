#pragma once

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#ifdef __linux__
#ifdef WilloRHI_BUILD_WAYLAND
#include <wayland-client.h>
#define VK_USE_PLATFORM_WAYLAND_KHR
#endif
#ifdef WilloRHI_BUILD_X11
#include <X11/Xlib.h>
#define VK_USE_PLATFORM_XLIB_KHR
#endif
#endif

#include "WilloRHI/Forward.hpp"
#include "WilloRHI/Types.hpp"

#include "WilloRHI/Sync.hpp"
#include "WilloRHI/Resources.hpp"

#include "WilloRHI/Device.hpp"
#include "WilloRHI/Swapchain.hpp"
#include "WilloRHI/CommandList.hpp"
#include "WilloRHI/Queue.hpp"
