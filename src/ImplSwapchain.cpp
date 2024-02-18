#include "ImplSwapchain.hpp"
#include "ImplDevice.hpp"

#include "ImplSync.hpp"

namespace WilloRHI
{
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
            _resizeRequested = true;
            device->LogMessage("Swapchain needs resize", false);
        }

        return _images[_currentImageIndex];
    }

    uint64_t ImplSwapchain::GetCurrentImageIndex() const
    {
        return _currentImageIndex;
    }

    BinarySemaphore const& ImplSwapchain::GetAcquireSemaphore()
    {
        return _imageSync[_frameNum % _framesInFlight];
    }

    bool ImplSwapchain::ResizeRequested() const {
        return _resizeRequested;
    }
}
