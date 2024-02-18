#include "ImplSwapchain.hpp"

#include "ImplSync.hpp"

namespace WilloRHI
{
    ImageId ImplSwapchain::AcquireNextImage()
    {
        _frameNum += 1;
        vkAcquireNextImageKHR(
            vkDevice,
            _vkSwapchain,
            1000000000,
            _imageSync[_frameNum % _framesInFlight].impl->vkSemaphore,
            nullptr,
            &_currentImageIndex
        );

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
}
