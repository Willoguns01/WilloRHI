#include "WilloRHI/WilloRHI.hpp"

#include "ImplDevice.hpp"
#include "ImplSwapchain.hpp"
#include "ImplSync.hpp"
#include "ImplCommandList.hpp"
#include "ImplQueue.hpp"

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
#include <VkBootstrap.h>
#include <vulkan/vk_enum_string_helper.h>

#include <functional>

namespace WilloRHI
{
    void ImplDevice::Init(const DeviceCreateInfo& createInfo) {
        if (createInfo.logCallback != nullptr)
            _loggingCallback = createInfo.logCallback;
        _doLogInfo = createInfo.logInfo;
        
        LogMessage("Initialising Device...", false);

        // TODO: split up into a few different functions to allow moving away from vkb

        vkb::InstanceBuilder instanceBuilder;
        auto inst_ret = instanceBuilder
            .set_app_name(createInfo.applicationName.data())
            .request_validation_layers(createInfo.validationLayers)
            .use_default_debug_messenger()
            .require_api_version(1, 3, 0)
            .build();

        vkb::Instance vkbInstance = inst_ret.value();
        _vkInstance = vkbInstance.instance;
        _vkDebugMessenger = vkbInstance.debug_messenger;

        VkPhysicalDeviceVulkan13Features features13 = {
            .synchronization2 = true,
            .dynamicRendering = true
        };

        VkPhysicalDeviceVulkan12Features features12 = {
            .descriptorIndexing = true,
            .descriptorBindingSampledImageUpdateAfterBind = true,
            .timelineSemaphore = true,
            .bufferDeviceAddress = true,
        };

        vkb::PhysicalDeviceSelector selector{vkbInstance};
        vkb::PhysicalDevice physicalDevice = selector
            .set_minimum_version(1, 3)
            .set_required_features_13(features13)
            .set_required_features_12(features12)
            .defer_surface_initialization()
            .select()
            .value();

        vkb::DeviceBuilder deviceBuilder{physicalDevice};
        vkb::Device vkbDevice = deviceBuilder.build().value();

        _vkbDevice = vkbDevice;
        _vkDevice = vkbDevice.device;
        _vkPhysicalDevice = physicalDevice.physical_device;

        VmaAllocatorCreateInfo allocatorInfo = {};
        allocatorInfo.physicalDevice = _vkPhysicalDevice;
        allocatorInfo.device = _vkDevice;
        allocatorInfo.instance = _vkInstance;
        allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
        
        vmaCreateAllocator(&allocatorInfo, &_allocator);

        SetupDescriptors(createInfo.resourceCounts);

        _vkQueueIndices[0] = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
        _vkQueueIndices[1] = vkbDevice.get_queue_index(vkb::QueueType::compute).value();
        _vkQueueIndices[2] = vkbDevice.get_queue_index(vkb::QueueType::transfer).value();

        _resources.images.device = this;
        _resources.imageViews.device = this;
        _resources.samplers.device = this;
        _resources.buffers.device = this;

        LogMessage("Initialised Device", false);
        
        if (createInfo.validationLayers)
            LogMessage("Validation layers are enabled", false);
    }

    Device Device::CreateDevice(const DeviceCreateInfo& createInfo)
    {
        Device newDevice;
        newDevice.impl = std::make_shared<ImplDevice>();
        newDevice.impl->Init(createInfo);
        return newDevice;
    }

    void ImplDevice::SetupDescriptors(const ResourceCountInfo& countInfo)
    {
        _resources.buffers = ResourceMap<BufferResource>(countInfo.bufferCount);
        _resources.images = ResourceMap<ImageResource>(countInfo.imageCount);
        _resources.imageViews = ResourceMap<ImageViewResource>(countInfo.imageViewCount);
        _resources.samplers = ResourceMap<SamplerResource>(countInfo.samplerCount);
    
        std::vector<VkDescriptorSetLayoutBinding> bindings = {
            VkDescriptorSetLayoutBinding {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = (uint32_t)countInfo.bufferCount,
                .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
                .pImmutableSamplers = nullptr
            },
            VkDescriptorSetLayoutBinding {
                .binding = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .descriptorCount = (uint32_t)countInfo.imageViewCount,
                .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
                .pImmutableSamplers = nullptr
            },
            VkDescriptorSetLayoutBinding {
                .binding = 2,
                .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                .descriptorCount = (uint32_t)countInfo.imageViewCount,
                .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
                .pImmutableSamplers = nullptr
            },
            VkDescriptorSetLayoutBinding {
                .binding = 3,
                .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
                .descriptorCount = (uint32_t)countInfo.samplerCount,
                .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
                .pImmutableSamplers = nullptr
            }
        };

        std::vector<VkDescriptorPoolSize> poolSizes;
        for (int i = 0; i < bindings.size(); i++) {
            poolSizes.push_back(VkDescriptorPoolSize{
                .type = bindings.at(i).descriptorType,
                .descriptorCount = bindings.at(i).descriptorCount
            });
        }

        VkDescriptorPoolCreateInfo poolInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
            .maxSets = 1,
            .poolSizeCount = (uint32_t)poolSizes.size(),
            .pPoolSizes = poolSizes.data()
        };

        ErrorCheck(vkCreateDescriptorPool(_vkDevice, &poolInfo, nullptr, &_globalDescriptors.pool));

        VkDescriptorSetLayoutCreateInfo setLayoutInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
            .bindingCount = (uint32_t)bindings.size(),
            .pBindings = bindings.data()
        };

        ErrorCheck(vkCreateDescriptorSetLayout(_vkDevice, &setLayoutInfo, nullptr, &_globalDescriptors.setLayout));

        VkDescriptorSetAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = nullptr,
            .descriptorPool = _globalDescriptors.pool,
            .descriptorSetCount = 1,
            .pSetLayouts = &_globalDescriptors.setLayout
        };

        ErrorCheck(vkAllocateDescriptorSets(_vkDevice, &allocInfo, &_globalDescriptors.descriptorSet));

        LogMessage("Allocated resource sets with the following counts:\n - "
            + std::to_string(countInfo.bufferCount) + " Buffers\n - "
            + std::to_string(countInfo.imageCount) + " Images\n - "
            + std::to_string(countInfo.imageViewCount) + " ImageViews\n - "
            + std::to_string(countInfo.samplerCount) + " Samplers", false);
    }

    void ImplDevice::SetupDefaultResources()
    {
        // default "error" texture, view, empty buffer, default sampler, etc

        LogMessage("Loaded default resources", false);
    }

    void Device::Cleanup() { impl->Cleanup(); }
    void ImplDevice::Cleanup()
    {
        WaitIdle();

        // above WaitIdle *should* mean all of the queues will collect everything that's pending
        // should be safe to annihilate everything
        CollectGarbage();

        for (int i = 0; i < _deviceQueues.size(); i++) {
            for (auto& pool : _deviceQueues.at(i).impl->_commandPools) {
                vkDestroyCommandPool(_vkDevice, pool.second, nullptr);
            }
            vkDestroySemaphore(_vkDevice, _deviceQueues.at(i).impl->_submissionTimeline.impl->vkSemaphore, nullptr);
        }

        vkDestroyDescriptorSetLayout(_vkDevice, _globalDescriptors.setLayout, nullptr);
        vkDestroyDescriptorPool(_vkDevice, _globalDescriptors.pool, nullptr);

        vmaDestroyAllocator(_allocator);
        vkDestroyDevice(_vkDevice, nullptr);

        vkb::destroy_debug_utils_messenger(_vkInstance, _vkDebugMessenger);
        vkDestroyInstance(_vkInstance, nullptr);
    }

    void Device::WaitIdle() const { impl->WaitIdle(); }
    void ImplDevice::WaitIdle() const
    {
        vkDeviceWaitIdle(_vkDevice);
    }

    BinarySemaphore Device::CreateBinarySemaphore() { return impl->CreateBinarySemaphore(); }
    BinarySemaphore ImplDevice::CreateBinarySemaphore()
    {
        BinarySemaphore newSemaphore = {};
        newSemaphore.impl = std::make_shared<ImplBinarySemaphore>();
        ImplBinarySemaphore* newImpl = newSemaphore.impl.get();

        VkSemaphoreCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0
        };

        ErrorCheck(vkCreateSemaphore(_vkDevice, &createInfo, nullptr, &newImpl->vkSemaphore));
        LogMessage("Created binary semaphore", false);
        return newSemaphore;
    }

    TimelineSemaphore Device::CreateTimelineSemaphore(uint64_t initialValue) {
        return impl->CreateTimelineSemaphore(initialValue); }
    TimelineSemaphore ImplDevice::CreateTimelineSemaphore(uint64_t initialValue)
    {
        TimelineSemaphore newSemaphore = {};
        newSemaphore.impl = std::make_shared<ImplTimelineSemaphore>();
        ImplTimelineSemaphore* newImpl = newSemaphore.impl.get();

        VkSemaphoreTypeCreateInfo typeInfo = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
            .pNext = nullptr,
            .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
            .initialValue = initialValue
        };

        VkSemaphoreCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = &typeInfo,
            .flags = 0
        };

        ErrorCheck(vkCreateSemaphore(_vkDevice, &createInfo, nullptr, &newImpl->vkSemaphore));
        LogMessage("Created timeline semaphore", false);
        return newSemaphore;
    }

    BufferId Device::CreateBuffer(const BufferCreateInfo& createInfo) {
        return impl->CreateBuffer(createInfo); }
    BufferId ImplDevice::CreateBuffer(const BufferCreateInfo& createInfo)
    {
        VkBufferCreateInfo bufferInfo = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .size = createInfo.size,
            .usage = BUFFER_USAGE_FLAGS,
            // apparently concurrent sharing mode has no affect with buffers?
            // need to test this vs. exclusive sharing with queue ownership transfer
            .sharingMode = VK_SHARING_MODE_CONCURRENT,
            .queueFamilyIndexCount = 3,
            .pQueueFamilyIndices = _vkQueueIndices
        };

        VmaAllocationCreateInfo allocInfo = {

        };

        return {};
    }

    void Device::DestroyBinarySemaphore(BinarySemaphore semaphore) {
        impl->DestroyBinarySemaphore(semaphore); }
    void ImplDevice::DestroyBinarySemaphore(BinarySemaphore semaphore) {
        vkDestroySemaphore(_vkDevice, semaphore.impl->vkSemaphore, nullptr);
    }

    void Device::DestroyTimelineSemaphore(TimelineSemaphore semaphore) {
        impl->DestroyTimelineSemaphore(semaphore); }
    void ImplDevice::DestroyTimelineSemaphore(TimelineSemaphore semaphore) {
        vkDestroySemaphore(_vkDevice, semaphore.impl->vkSemaphore, nullptr);
    }

    void Device::DestroySwapchain(Swapchain swapchain) {
        impl->DestroySwapchain(swapchain); }
    void ImplDevice::DestroySwapchain(Swapchain swapchain)
    {
        for (int i = 0; i < swapchain.impl->_framesInFlight; i++) {
            _resources.images.freeSlotQueue.enqueue(swapchain.impl->_images.at(i).id);
        }

        vkDestroySwapchainKHR(_vkDevice, swapchain.impl->_vkSwapchain, nullptr);
        vkDestroySurfaceKHR(_vkInstance, swapchain.impl->_vkSurface, nullptr);

        for (int i = 0; i < swapchain.impl->_framesInFlight; i++) {
            DestroyBinarySemaphore(swapchain.impl->_imageSync[i]);
        }
    }

    void Device::WaitSemaphore(TimelineSemaphore semaphore, uint64_t value, uint64_t timeout) {
        impl->WaitSemaphore(semaphore, value, timeout); }
    void ImplDevice::WaitSemaphore(TimelineSemaphore semaphore, uint64_t value, uint64_t timeout)
    {
        VkSemaphoreWaitInfo waitInfo = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
            .pNext = nullptr,
            .semaphoreCount = 1,
            .pSemaphores = &semaphore.impl->vkSemaphore,
            .pValues = &value
        };

        ErrorCheck(vkWaitSemaphores(_vkDevice, &waitInfo, timeout));
    }

    uint64_t Device::GetSemaphoreValue(TimelineSemaphore semaphore) {
        return impl->GetSemaphoreValue(semaphore); }
    uint64_t ImplDevice::GetSemaphoreValue(TimelineSemaphore semaphore)
    {
        uint64_t result = 0;
        ErrorCheck(vkGetSemaphoreCounterValue(_vkDevice, semaphore.impl->vkSemaphore, &result));
        return result;
    }

    void Device::CollectGarbage() { impl->CollectGarbage(); }
    void ImplDevice::CollectGarbage()
    {
        std::unique_lock<std::shared_mutex> lock(_resources.resourcesMutex);

        for (int i = 0; i < _deviceQueues.size(); i++) {
            _deviceQueues.at(i).CollectGarbage();
        }
    }

    VkSurfaceKHR ImplDevice::CreateSurface(NativeWindowHandle handle)
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

        ErrorCheck(vkCreateWin32SurfaceKHR(_vkInstance, &surfaceInfo, nullptr, &newSurface));
        LogMessage("Created Win32 window surface", false);
#endif

        // I have no idea if this linux stuff works, need to check
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

            ErrorCheck(vkCreateWaylandSurfaceKHR(_vkInstance, &surfaceInfo, nullptr, &newSurface));
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

            ErrorCheck(vkCreateXlibSurfaceKHR(_vkInstance, &surfaceInfo, nullptr, &newSurface));
            LogMessage("Created X11 window surface", false);
        }
#endif
#endif
        return newSurface;
    }

    void Device::LogMessage(const std::string& message, bool error) {
        impl->LogMessage(message, error);
    }

    void Device::ErrorCheck(uint64_t errorCode) {
        impl->ErrorCheck(errorCode);
    }

    void ImplDevice::LogMessage(const std::string& message, bool error)
    {
        if (_loggingCallback) {
            if (error) {
                _loggingCallback(std::string("WilloRHI Error: ") + message);
            } else {
                if (_doLogInfo)
                    _loggingCallback(std::string("WilloRHI Info: ") + message);
            }
        }
    }

    void ImplDevice::ErrorCheck(uint64_t errorCode)
    {
        VkResult result = static_cast<VkResult>(errorCode);
        if (result != VK_SUCCESS && _loggingCallback) {
            LogMessage(std::string("Vulkan Error: ") + std::string(string_VkResult(result)));
        }
    }
}
