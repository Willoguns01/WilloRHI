#include "ImplSwapchain.hpp"
#include "ImplDevice.hpp"

#include "WilloRHI/Resources.hpp"

#include <VkBootstrap.h>

#include "ImplSync.hpp"

namespace WilloRHI
{
    Swapchain Swapchain::Create(Device device, const SwapchainCreateInfo& createInfo)
    {
        Swapchain newSwapchain;
        newSwapchain.impl = std::make_shared<ImplSwapchain>();
        newSwapchain.impl->Init(device, createInfo);
        return newSwapchain;
    }

    void ImplSwapchain::Init(Device pDevice, const SwapchainCreateInfo& createInfo)
    {
        std::shared_ptr<ImplDevice> deviceImpl = pDevice.impl;
        device = deviceImpl;

        vkDevice = device->_vkDevice;
        vkPhysicalDevice = device->_vkPhysicalDevice;

        _vkSurface = device->CreateSurface(createInfo.windowHandle);

        vkb::SwapchainBuilder swapchainBuilder{vkPhysicalDevice, vkDevice, _vkSurface};

        vkb::Swapchain vkbSwapchain = swapchainBuilder
            .set_desired_format(VkSurfaceFormatKHR{.format = static_cast<VkFormat>(createInfo.format), .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
            .set_desired_present_mode(static_cast<VkPresentModeKHR>(createInfo.presentMode))
            .set_desired_extent(createInfo.width, createInfo.height)
            .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            .set_required_min_image_count(createInfo.framesInFlight)
            .build()
            .value();

        _vkSwapchain = vkbSwapchain.swapchain;
        _swapchainExtent = Extent2D{.width = createInfo.width, .height = createInfo.height};
        _swapchainFormat = createInfo.format;
        _framesInFlight = createInfo.framesInFlight;

        std::vector<VkImage> vkImages = vkbSwapchain.get_images().value();

        _images.reserve((size_t)createInfo.framesInFlight);

        for (uint32_t i = 0; i < createInfo.framesInFlight; i++)
        {
            uint64_t imageId = device->_resources.images.Allocate();
            device->_resources.images.At(imageId) = {.image = vkImages[i], .allocation = VK_NULL_HANDLE, .createInfo = {}};
            _images.push_back(ImageId{.id = imageId});

            _imageSync.push_back(device->CreateBinarySemaphore());
        }
        
        device->LogMessage("Created swapchain", false);
    }

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
            device->_resources.images.Free(_images.at(i).id);
            uint64_t imageId = device->_resources.images.Allocate();

            device->_resources.images.At(imageId) = {.image = vkImages[i], .allocation = VK_NULL_HANDLE, .createInfo = {}};
            _images.at(i) = ImageId{.id = imageId};
        }

        device->LogMessage("Resized swapchain", false);
    }
}
