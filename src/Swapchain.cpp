#include "ImplSwapchain.hpp"

namespace WilloRHI
{
    Swapchain::Swapchain()
        : impl(new ImplSwapchain) {}

    ImageId Swapchain::AcquireNextImage() {
        return impl->AcquireNextImage();
    }

    uint64_t Swapchain::GetCurrentImageIndex() const {
        return impl->GetCurrentImageIndex();
    }

    BinarySemaphore const& Swapchain::GetAcquireSemaphore() {
        return impl->GetAcquireSemaphore();
    }
}
