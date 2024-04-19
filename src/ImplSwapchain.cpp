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

#include "ImplSwapchain.hpp"
#include "ImplResources.hpp"

#include <VkBootstrap.h>

#include "WilloRHI/Sync.hpp"
//#include "ImplSync.hpp"

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
        device = pDevice;

        vkDevice = (VkDevice)device.GetDeviceNativeHandle();
        vkPhysicalDevice = (VkPhysicalDevice)device.GetPhysicalDeviceNativeHandle();

        _vkSurface = CreateSurface(createInfo.windowHandle);

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

        DeviceResources* resources = (DeviceResources*)device.GetDeviceResources();

        for (uint32_t i = 0; i < createInfo.framesInFlight; i++)
        {
            uint64_t imageId = resources->images.Allocate();
            resources->images.At(imageId) = {.image = vkImages[i], .allocation = VK_NULL_HANDLE, .createInfo = {}};
            _images.push_back(ImageId{.id = imageId});

            _imageSync.push_back(WilloRHI::BinarySemaphore::Create(pDevice));
        }
        
        device.LogMessage("Created swapchain", false);
    }

    ImplSwapchain::~ImplSwapchain() {
        Cleanup();
    }

    void ImplSwapchain::Cleanup() {
        DeviceResources* resources = (DeviceResources*)device.GetDeviceResources();
        for (int i = 0; i < _framesInFlight; i++) {
            resources->images.freeSlotQueue.enqueue(_images.at(i).id);
        }

        vkDestroySwapchainKHR(vkDevice, _vkSwapchain, nullptr);
        vkDestroySurfaceKHR((VkInstance)device.GetInstanceNativeHandle(), _vkSurface, nullptr);
    }

    VkSurfaceKHR ImplSwapchain::CreateSurface(NativeWindowHandle handle)
    {
        VkSurfaceKHR newSurface = VK_NULL_HANDLE;
        
#ifdef _WIN32
        VkWin32SurfaceCreateInfoKHR surfaceInfo = {
            .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
            .pNext = nullptr,
            .flags = 0,
            .hinstance = GetModuleHandleA(nullptr),
            .hwnd = static_cast<HWND>(handle)
        };

        device.ErrorCheck(vkCreateWin32SurfaceKHR((VkInstance)device.GetInstanceNativeHandle(), &surfaceInfo, nullptr, &newSurface));
        device.LogMessage("Created Win32 window surface", false);
#endif

#ifdef __linux__
#ifdef WilloRHI_BUILD_WAYLAND
        if (std::getenv("WAYLAND_DISPLAY")) {
            VkWaylandSurfaceCreateInfoKHR surfaceInfo = {
                .sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
                .pNext = nullptr,
                .flags = 0,
                .display = wl_display_connect(nullptr),
                .surface = static_cast<wl_surface*>(handle)
            };

            ErrorCheck(vkCreateWaylandSurfaceKHR((VkInstance)device.GetInstanceNativeHandle(), &surfaceInfo, nullptr, &newSurface));
            LogMessage("Created Wayland window surface", false);
        }
#endif
#ifdef WilloRHI_BUILD_X11
        if (std::getenv("DISPLAY")) {
            VkXlibSurfaceCreateInfoKHR surfaceInfo = {
                .sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
                .pNext = nullptr,
                .flags = 0,
                .dpy = XOpenDisplay(nullptr),
                .window = reinterpret_cast<Window>(handle),
            };

            ErrorCheck(vkCreateXlibSurfaceKHR((VkInstance)device.GetInstanceNativeHandle(), &surfaceInfo, nullptr, &newSurface));
            LogMessage("Created X11 window surface", false);
        }
#endif
#endif
        return newSurface;
    }

    ImageId Swapchain::AcquireNextImage() { return impl->AcquireNextImage(); }
    ImageId ImplSwapchain::AcquireNextImage()
    {
        _frameNum += 1;
        VkResult result = vkAcquireNextImageKHR(
            vkDevice,
            _vkSwapchain,
            1000000000,
            (VkSemaphore)_imageSync[_frameNum % _framesInFlight].GetNativeHandle(),
            nullptr,
            &_currentImageIndex
        );

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            _needResize = true;
            device.LogMessage("Swapchain needs resize", false);
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
        device.WaitIdle();

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

        DeviceResources* resources = (DeviceResources*)device.GetDeviceResources();

        for (int32_t i = 0; i < _framesInFlight; i++)
        {
            resources->images.Free(_images.at(i).id);
            uint64_t imageId = resources->images.Allocate();

            resources->images.At(imageId) = {.image = vkImages[i], .allocation = VK_NULL_HANDLE, .createInfo = {}};
            _images.at(i) = ImageId{.id = imageId};
        }

        device.LogMessage("Resized swapchain", false);
    }

    void* Swapchain::GetNativeHandle() const { return impl->GetNativeHandle(); }
    void* ImplSwapchain::GetNativeHandle() const {
        return static_cast<void*>(_vkSwapchain);
    }

    void Swapchain::SetNeedsResize(bool value) { impl->SetNeedsResize(value); }
    void ImplSwapchain::SetNeedsResize(bool value) {
        _needResize = value;
    }
}
