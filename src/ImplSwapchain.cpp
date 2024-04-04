#include "ImplSwapchain.hpp"
#include "ImplDevice.hpp"

#include <VkBootstrap.h>

#include "ImplSync.hpp"

namespace WilloRHI
{
    ImageId Swapchain::AcquireNextImage() { return impl->AcquireNextImage(); }
    ImageId ImplSwapchain::AcquireNextImage()
    {
        _frameNum += 1;
        VkResult result = vkAcquireNextImageKHR(
            vkDevice,
            _vkSwapchain,
            1000000000,
            _imageSync[_frameNum % _framesInFlight].impl->vkSemaphore,
            nullptr,
            &_currentImageIndex
        );

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            _needResize = true;
            device->LogMessage("Swapchain needs resize", false);
        }

        return _images[_currentImageIndex];
    }

    uint64_t Swapchain::GetCurrentImageIndex() const { return impl->GetCurrentImageIndex(); }
    uint64_t ImplSwapchain::GetCurrentImageIndex() const
    {
        return _currentImageIndex;
    }

    BinarySemaphore const& Swapchain::GetAcquireSemaphore() { return impl->GetAcquireSemaphore(); }
    BinarySemaphore const& ImplSwapchain::GetAcquireSemaphore()
    {
        return _imageSync[_frameNum % _framesInFlight];
    }

    bool Swapchain::NeedsResize() const { return impl->NeedsResize(); }
    bool ImplSwapchain::NeedsResize() const {
        return _needResize;
    }

    void Swapchain::Resize(uint32_t width, uint32_t height, PresentMode presentMode) {
        impl->Resize(width, height, presentMode); }
    void ImplSwapchain::Resize(uint32_t width, uint32_t height, PresentMode presentMode)
    {
        device->WaitIdle();

        vkDestroySwapchainKHR(vkDevice, _vkSwapchain, nullptr);

        vkb::SwapchainBuilder builder{vkPhysicalDevice, vkDevice, _vkSurface};

        vkb::Swapchain vkbSwapchain = builder
            .set_desired_format(VkSurfaceFormatKHR{.format = static_cast<VkFormat>(_swapchainFormat), .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
            .set_desired_present_mode(static_cast<VkPresentModeKHR>(presentMode))
            .set_desired_extent(width, height)
            .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            .set_required_min_image_count(_framesInFlight)
            .build()
            .value();

        _vkSwapchain = vkbSwapchain.swapchain;
        _swapchainExtent = Extent2D{.width = width, .height = height};

        std::vector<VkImage> vkImages = vkbSwapchain.get_images().value();

        for (int32_t i = 0; i < _framesInFlight; i++)
        {
            device->_resources.images.freeSlotQueue.enqueue(_images[i].id);
            _images[i] = device->AddImageResource(vkImages[i], nullptr, {});
        }

        device->LogMessage("Resized swapchain", false);
    }
}
