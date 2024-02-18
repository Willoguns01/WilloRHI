#include "ImplDevice.hpp"
#include "ImplSwapchain.hpp"
#include "ImplSync.hpp"
#include "ImplCommandList.hpp"

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <VkBootstrap.h>

#include <vulkan/vk_enum_string_helper.h>

namespace WilloRHI
{
    Device ImplDevice::CreateDevice(const DeviceCreateInfo& createInfo)
    {
        Device newDevice;
        ImplDevice* impl = newDevice.impl.get();

        if (createInfo.logCallback != nullptr)
            impl->_loggingCallback = createInfo.logCallback;
        impl->_doLogInfo = createInfo.logInfo;

        impl->LogMessage("Initialising Device...", false);

        // TODO: split up into a few different functions to allow moving away from vkb

        vkb::InstanceBuilder instanceBuilder;
        auto inst_ret = instanceBuilder
            .set_app_name(createInfo.applicationName.data())
            .request_validation_layers(createInfo.validationLayers)
            .use_default_debug_messenger()
            .require_api_version(1, 3, 0)
            .build();

        vkb::Instance vkbInstance = inst_ret.value();
        impl->_vkInstance = vkbInstance.instance;
        impl->_vkDebugMessenger = vkbInstance.debug_messenger;

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

        impl->_vkbDevice = vkbDevice;
        impl->_vkDevice = vkbDevice.device;
        impl->_vkPhysicalDevice = physicalDevice.physical_device;

        VmaAllocatorCreateInfo allocatorInfo = {};
        allocatorInfo.physicalDevice = impl->_vkPhysicalDevice;
        allocatorInfo.device = impl->_vkDevice;
        allocatorInfo.instance = impl->_vkInstance;
        allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
        
        vmaCreateAllocator(&allocatorInfo, &impl->_allocator);

        impl->SetupQueues(vkbDevice);

        impl->SetupDescriptors(createInfo.resourceCounts);

        impl->_maxFramesInFlight = createInfo.maxFramesInFlight;

        for (uint32_t i = 0; i < createInfo.maxFramesInFlight; i++) {
            impl->_deletionQueue.frameQueues.push_back(WilloRHI::DeletionQueues::PerFrame{});
        }

        impl->LogMessage("Initialised Device", false);
        
        if (createInfo.validationLayers)
            impl->LogMessage("Validation layers are enabled", false);

        return newDevice;
    }

    void ImplDevice::SetupQueues(vkb::Device vkbDevice)
    {
        LogMessage("Getting device queues", false);

        _graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
        _graphicsQueueIndex = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
        LogMessage("Initialised graphics queue with index " + std::to_string(_graphicsQueueIndex), false);

        _computeQueue = vkbDevice.get_queue(vkb::QueueType::compute).value();
        _computeQueueIndex = vkbDevice.get_queue_index(vkb::QueueType::compute).value();
        LogMessage("Initialised compute queue with index " + std::to_string(_computeQueueIndex), false);

        _transferQueue = vkbDevice.get_queue(vkb::QueueType::transfer).value();
        _transferQueueIndex = vkbDevice.get_queue_index(vkb::QueueType::transfer).value();
        LogMessage("Initialised transfer queue with index " + std::to_string(_transferQueueIndex), false);

        LogMessage("Initialised device queues", false);
    }

    void ImplDevice::SetupDescriptors(const ResourceCountInfo& countInfo)
    {
        _resources.buffers = ResourceMap<BufferResource>(countInfo.bufferCount);
        _resources.images = ResourceMap<ImageResource>(countInfo.imageCount);
        _resources.imageViews = ResourceMap<ImageViewResource>(countInfo.imageViewCount);
        _resources.samplers = ResourceMap<SamplerResource>(countInfo.samplerCount);
    
        LogMessage("Initialised resource sets with the following counts:\n - "
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

    void ImplDevice::Cleanup()
    {
        WaitIdle();

        for (uint32_t i = 0; i < _maxFramesInFlight; i++) {
            CollectGarbage();
            NextFrame();
        }

        for (auto& pool : _commandListPool.commandPools) {
            vkDestroyCommandPool(_vkDevice, pool.second, nullptr);
        }

        vmaDestroyAllocator(_allocator);
        vkDestroyDevice(_vkDevice, nullptr);

        vkb::destroy_debug_utils_messenger(_vkInstance, _vkDebugMessenger);
        vkDestroyInstance(_vkInstance, nullptr);
    }

    void ImplDevice::WaitIdle() const
    {
        vkDeviceWaitIdle(_vkDevice);
    }

    BinarySemaphore ImplDevice::CreateBinarySemaphore()
    {
        BinarySemaphore newSemaphore = {};
        newSemaphore.impl = std::make_shared<ImplBinarySemaphore>();
        ImplBinarySemaphore* impl = newSemaphore.impl.get();

        VkSemaphoreCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0
        };

        ErrorCheck(vkCreateSemaphore(_vkDevice, &createInfo, nullptr, &impl->vkSemaphore));
        LogMessage("Created binary semaphore", false);
        return newSemaphore;
    }

    TimelineSemaphore ImplDevice::CreateTimelineSemaphore(uint64_t initialValue)
    {
        TimelineSemaphore newSemaphore = {};
        newSemaphore.impl = std::make_shared<ImplTimelineSemaphore>();
        ImplTimelineSemaphore* impl = newSemaphore.impl.get();

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

        ErrorCheck(vkCreateSemaphore(_vkDevice, &createInfo, nullptr, &impl->vkSemaphore));
        LogMessage("Created timeline semaphore", false);
        return newSemaphore;
    }

    Swapchain ImplDevice::CreateSwapchain(const SwapchainCreateInfo& createInfo, Device* parent)
    {
        LogMessage("Creating swapchain...", false);
        Swapchain newSwapchain;
        ImplSwapchain* swapImpl = newSwapchain.impl.get();

        swapImpl->device = this;
        swapImpl->vkDevice = _vkDevice;

        VkSurfaceKHR vkSurface = CreateSurface(createInfo.windowHandle);

        vkb::SwapchainBuilder swapchainBuilder{_vkPhysicalDevice, _vkDevice, vkSurface};

        vkb::Swapchain vkbSwapchain = swapchainBuilder
            .set_desired_format(VkSurfaceFormatKHR{.format = static_cast<VkFormat>(createInfo.format), .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
            .set_desired_present_mode(static_cast<VkPresentModeKHR>(createInfo.presentMode))
            .set_desired_extent(createInfo.width, createInfo.height)
            .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            .set_required_min_image_count(createInfo.framesInFlight)
            .build()
            .value();

        swapImpl->_vkSurface = vkSurface;
        swapImpl->_vkSwapchain = vkbSwapchain.swapchain;
        swapImpl->_swapchainExtent = Extent2D{.width = createInfo.width, .height = createInfo.height};
        swapImpl->_swapchainFormat = createInfo.format;
        swapImpl->_framesInFlight = createInfo.framesInFlight;

        std::vector<VkImage> vkImages = vkbSwapchain.get_images().value();

        swapImpl->_images.reserve((size_t)createInfo.framesInFlight);

        for (uint32_t i = 0; i < createInfo.framesInFlight; i++)
        {
            swapImpl->_images.push_back(AddImageResource(vkImages[i], VK_NULL_HANDLE, {}));
            swapImpl->_imageSync.push_back(CreateBinarySemaphore());
        }
        
        LogMessage("Created swapchain", false);

        return newSwapchain;
    }

    CommandList ImplDevice::GetCommandList()
    {
        std::thread::id threadId = std::this_thread::get_id();
        CommandListPool::MapType* map = &_commandListPool.primaryBuffers;

        // create the pool if it doesn't exist
        if (_commandListPool.commandPools.find(threadId) == _commandListPool.commandPools.end()) {
            VkCommandPool newPool = VK_NULL_HANDLE;

            VkCommandPoolCreateInfo poolInfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .pNext = nullptr,
                .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = _graphicsQueueIndex
            };

            ErrorCheck(vkCreateCommandPool(_vkDevice, &poolInfo, nullptr, &newPool));

            _commandListPool.commandPools.insert({ threadId, newPool });

            _commandListPool.primaryBuffers.insert({ threadId, moodycamel::ConcurrentQueue<CommandList>()});
        
            LogMessage("Created new CommandPool for thread " + std::to_string(std::hash<std::thread::id>()(threadId)), false);
        }

        CommandList commandList;
        if (!map->at(threadId).try_dequeue(commandList)) {
            commandList.impl = std::make_shared<ImplCommandList>();

            VkCommandBufferAllocateInfo allocInfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .pNext = nullptr,
                .commandPool = _commandListPool.commandPools.at(threadId),
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1
            };

            ErrorCheck(vkAllocateCommandBuffers(_vkDevice, &allocInfo, &commandList.impl->_vkCommandBuffer));
        
            commandList.impl->_device = this;
            commandList.impl->_threadId = std::this_thread::get_id();

            LogMessage("Allocated CommandList for thread " + std::to_string(std::hash<std::thread::id>()(threadId))
                + " on frame " + std::to_string(_frameNum % _maxFramesInFlight), false);
        }

        return commandList;
    }

    void ImplDevice::DestroyBinarySemaphore(BinarySemaphore semaphore) {
        _deletionQueue.frameQueues.at(_frameNum % _maxFramesInFlight)
            .semaphoreQueue.enqueue(semaphore);
    }

    void ImplDevice::DestroyTimelineSemaphore(TimelineSemaphore semaphore) {
        _deletionQueue.frameQueues.at(_frameNum % _maxFramesInFlight)
            .timelineSemaphoreQueue.enqueue(semaphore);
    }

    void ImplDevice::DestroySwapchain(Swapchain swapchain)
    {
        _deletionQueue.frameQueues.at(_frameNum % _maxFramesInFlight)
            .swapchainQueue.enqueue(swapchain);

        for (int i = 0; i < swapchain.impl->_framesInFlight; i++) {
            _deletionQueue.frameQueues.at(_frameNum % _maxFramesInFlight)
                .semaphoreQueue.enqueue(swapchain.impl->_imageSync.at(i));
        }
    }

    void ImplDevice::NextFrame()
    {
        _frameNum += 1;
    }

    uint64_t ImplDevice::GetFrameNum()
    {
        return _frameNum;
    }

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

    uint64_t ImplDevice::GetSemaphoreValue(TimelineSemaphore semaphore)
    {
        uint64_t result = 0;
        ErrorCheck(vkGetSemaphoreCounterValue(_vkDevice, semaphore.impl->vkSemaphore, &result));
        return result;
    }

    void ImplDevice::QueuePresent(const PresentInfo& presentInfo)
    {
        std::vector<VkSemaphore> waitSemaphores;
        for (int i = 0; i < presentInfo.waitSemaphores.size(); i++) {
            waitSemaphores.push_back(presentInfo.waitSemaphores[i].impl->vkSemaphore);
        }

        uint32_t currentIndex = (uint32_t)presentInfo.swapchain->GetCurrentImageIndex();

        VkPresentInfoKHR info = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext = nullptr,
            .waitSemaphoreCount = (uint32_t)presentInfo.waitSemaphores.size(),
            .pWaitSemaphores = waitSemaphores.data(),
            .swapchainCount = 1,
            .pSwapchains = &presentInfo.swapchain->impl->_vkSwapchain,
            .pImageIndices = &currentIndex,
            .pResults = {}
        };

        VkResult result = vkQueuePresentKHR(_graphicsQueue, &info);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            presentInfo.swapchain->impl->_resizeRequested = true;
            LogMessage("Present needs resize", false);
        }
    }

    void ImplDevice::QueueSubmit(const CommandSubmitInfo& submitInfo)
    {
        int64_t commandListCount = submitInfo.commandLists.size();
        std::vector<VkCommandBuffer> cmdBuffers;

        for (int i = 0; i < commandListCount; i++) {
            cmdBuffers.push_back(submitInfo.commandLists.at(i).impl->_vkCommandBuffer);
        }

        std::vector<VkSemaphore> waitSemaphores;
        std::vector<uint64_t> waitValues;
        std::vector<VkPipelineStageFlags> waitStageFlags;
        std::vector<VkSemaphore> signalSemaphores;
        std::vector<uint64_t> signalValues;

        for (int i = 0; i < submitInfo.waitTimelineSemaphores.size(); i++) {
            waitSemaphores.push_back(submitInfo.waitTimelineSemaphores[i].first.impl->vkSemaphore);
            waitValues.push_back(submitInfo.waitTimelineSemaphores[i].second);
            // TODO: would providing an API for this give opportunities for optimisation?
            waitStageFlags.push_back(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        }

        for (int i = 0; i < submitInfo.waitBinarySemaphores.size(); i++) {
            waitSemaphores.push_back(submitInfo.waitBinarySemaphores[i].impl->vkSemaphore);
            waitStageFlags.push_back(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        }

        for (int i = 0; i < submitInfo.signalTimelineSemaphores.size(); i++) {
            signalSemaphores.push_back(submitInfo.signalTimelineSemaphores[i].first.impl->vkSemaphore);
            signalValues.push_back(submitInfo.signalTimelineSemaphores[i].second);
        }

        for (int i = 0; i < submitInfo.signalBinarySemaphores.size(); i++) {
            signalSemaphores.push_back(submitInfo.signalBinarySemaphores[i].impl->vkSemaphore);
        }

        VkTimelineSemaphoreSubmitInfo timelineSubmitInfo = {
            .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
            .pNext = nullptr,
            .waitSemaphoreValueCount = (uint32_t)waitSemaphores.size(),
            .pWaitSemaphoreValues = waitValues.data(),
            .signalSemaphoreValueCount = (uint32_t)signalSemaphores.size(),
            .pSignalSemaphoreValues = signalValues.data()
        };
        
        VkSubmitInfo info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = &timelineSubmitInfo,
            .waitSemaphoreCount = (uint32_t)waitSemaphores.size(),
            .pWaitSemaphores = waitSemaphores.data(),
            .pWaitDstStageMask = waitStageFlags.data(),
            .commandBufferCount = (uint32_t)commandListCount,
            .pCommandBuffers = cmdBuffers.data(),
            .signalSemaphoreCount = (uint32_t)signalSemaphores.size(),
            .pSignalSemaphores = signalSemaphores.data()
        };

        VkQueue submissionQueue = VK_NULL_HANDLE;
        switch (submitInfo.queueType)
        {
        case SubmitQueueType::GRAPHICS:
            submissionQueue = _graphicsQueue;
            break;
        case SubmitQueueType::COMPUTE:
            submissionQueue = _computeQueue;
            break;
        case SubmitQueueType::TRANSFER:
            submissionQueue = _transferQueue;
            break;
        }

        ErrorCheck(vkQueueSubmit(submissionQueue, 1, &info, VK_NULL_HANDLE));

        if (submitInfo.syncToTimeline)
            _deletionQueue.frameQueues.at(_frameNum % _maxFramesInFlight).commandLists.enqueue_bulk(submitInfo.commandLists.begin(), commandListCount);
    }

    // is there even a point to this?
    // it seems to just overcomplicate code and add lines??
    // TODO: remove and see if it makes any difference in readability
    template <typename Handle_T, typename Resource_T>
    void DestroyDeviceResources(ResourceMap<Resource_T>* resourceMap,
        moodycamel::ConcurrentQueue<Handle_T>* deletionQueue, 
        VmaAllocator allocator, VkDevice device,
        void(*func)(Handle_T, Resource_T*, VmaAllocator, VkDevice)) 
    {
        Handle_T handle{};
        while (deletionQueue->try_dequeue(handle)) {
            Resource_T* rsrc = &resourceMap->resources[handle.id];
            func(handle, rsrc, allocator, device);
            resourceMap->freeSlotQueue.enqueue(handle.id);
        }
    }

    void ImplDevice::CollectGarbage()
    {
        int64_t frameIndex = _frameNum % _maxFramesInFlight;
        DeletionQueues::PerFrame& queue = _deletionQueue.frameQueues.at(frameIndex);

        DestroyDeviceResources<BufferId, BufferResource>(
            &_resources.buffers, &queue.bufferQueue, _allocator, _vkDevice,
            [](BufferId handle, BufferResource* resource, VmaAllocator allocator, VkDevice device) {
                vmaDestroyBuffer(allocator, resource->buffer, resource->allocation);
            });

        DestroyDeviceResources<ImageId, ImageResource>(
            &_resources.images, &queue.imageQueue, _allocator, _vkDevice,
            [](ImageId handle, ImageResource* resource, VmaAllocator allocator, VkDevice device) {
                vmaDestroyImage(allocator, resource->image, resource->allocation);
            });

        DestroyDeviceResources<ImageViewId, ImageViewResource>(
            &_resources.imageViews, &queue.imageViewQueue, _allocator, _vkDevice,
            [](ImageViewId handle, ImageViewResource* resource, VmaAllocator allocator, VkDevice device) {
                vkDestroyImageView(device, resource->imageView, nullptr);
            });

        DestroyDeviceResources<SamplerId, SamplerResource>(
            &_resources.samplers, &queue.samplerQueue, _allocator, _vkDevice,
            [](SamplerId handle, SamplerResource* resource, VmaAllocator allocator, VkDevice device) {
                vkDestroySampler(device, resource->sampler, nullptr);
            });

        CommandList cmdList;
        while (queue.commandLists.try_dequeue(cmdList)) {
            vkResetCommandBuffer(cmdList.impl->_vkCommandBuffer, 0);
            CommandListPool::MapType* map = &_commandListPool.primaryBuffers;
            map->at(cmdList.impl->_threadId).enqueue(cmdList);
        }

        BinarySemaphore binSemaphore{};
        while (queue.semaphoreQueue.try_dequeue(binSemaphore)) {
            vkDestroySemaphore(_vkDevice, binSemaphore.impl->vkSemaphore, nullptr);
        }

        TimelineSemaphore timSemaphore{};
        while (queue.timelineSemaphoreQueue.try_dequeue(timSemaphore)) {
            vkDestroySemaphore(_vkDevice, timSemaphore.impl->vkSemaphore, nullptr);
        }

        Swapchain swapchain{};
        while (queue.swapchainQueue.try_dequeue(swapchain)) {
            for (int i = 0; i < swapchain.impl->_framesInFlight; i++) {
                _resources.images.freeSlotQueue.enqueue(swapchain.impl->_images.at(i).id);
            }

            vkDestroySwapchainKHR(_vkDevice, swapchain.impl->_vkSwapchain, nullptr);
            vkDestroySurfaceKHR(_vkInstance, swapchain.impl->_vkSurface, nullptr);
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

        vkCreateWin32SurfaceKHR(_vkInstance, &surfaceInfo, nullptr, &newSurface);
#endif

#ifdef __linux__
#ifdef WilloRHI_BUILD_WAYLAND
#endif
#ifdef WilloRHI_BUILD_X11
#endif
#endif
        LogMessage("Created Vulkan surface", false);
        return newSurface;
    }

    ImageId ImplDevice::AddImageResource(VkImage image, VmaAllocation allocation, const ImageCreateInfo& createInfo)
    {
        // TODO: id 0 is just a stinky little error texture
        ImageId newId = {};
        newId.id = 0;

        uint64_t newSlot = 0;
        if (!_resources.images.freeSlotQueue.try_dequeue(newSlot)) {
            LogMessage("No free Image resource slots");
            return newId;
        }

        _resources.images.resources[newSlot] = {
            .image = image,
            .allocation = allocation,
            .createInfo = createInfo
        };

        newId.id = newSlot;

        LogMessage("Added vulkan image to resource index " + std::to_string(newId.id), false);
        return newId;
    }

    ImageViewId ImplDevice::AddImageViewResource(VkImageView view, const ImageViewCreateInfo& createInfo)
    {
        ImageViewId newId = {};
        newId.id = 0;

        uint64_t newSlot = 0;
        if (!_resources.imageViews.freeSlotQueue.try_dequeue(newSlot)) {
            LogMessage("No free ImageView resource slots");
            return newId;
        }

        _resources.imageViews.resources[newSlot] = {
            .imageView = view,
            .createInfo = createInfo
        };

        newId.id = newSlot;

        LogMessage("Added vulkan imageview to resources", false);
        return newId;
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

    void ImplDevice::ErrorCheck(VkResult result)
    {
        if (result != VK_SUCCESS && _loggingCallback) {
            LogMessage(std::string("Vulkan Error: ") + std::string(string_VkResult(result)));
        }
    }
}
