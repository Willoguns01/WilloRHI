#pragma once

#include "WilloRHI/Swapchain.hpp"

#include <vulkan/vulkan.h>
#include <vector>

namespace WilloRHI
{
    struct ImplSwapchain
    {
        Device device;
        VkDevice vkDevice = VK_NULL_HANDLE;
        VkPhysicalDevice vkPhysicalDevice = VK_NULL_HANDLE;

        std::vector<ImageId> _images;
        std::vector<BinarySemaphore> _imageSync;
        int32_t _framesInFlight = 0;

        VkSurfaceKHR _vkSurface = VK_NULL_HANDLE;
        VkSwapchainKHR _vkSwapchain = VK_NULL_HANDLE;
        Extent2D _swapchainExtent = {};

        Format _swapchainFormat = Format::UNDEFINED;

        int64_t _frameNum = -1;
        uint32_t _currentImageIndex = 0;

        bool _needResize = false;

        void Init(Device device, const SwapchainCreateInfo& createInfo);

        ~ImplSwapchain();
        void Cleanup();

        VkSurfaceKHR CreateSurface(NativeWindowHandle handle);

        ImageId AcquireNextImage();
        uint64_t GetCurrentImageIndex() const;

        BinarySemaphore const& GetAcquireSemaphore();

        bool NeedsResize() const;
        void Resize(uint32_t width, uint32_t height, PresentMode presentMode);

        void* GetNativeHandle() const;
        void SetNeedsResize(bool value);
    };
}
