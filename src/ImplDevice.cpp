#include "ImplDevice.hpp"
#include "ImplSwapchain.hpp"
#include "ImplSync.hpp"
#include "ImplCommandList.hpp"

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

        SetupQueues(vkbDevice);

        SetupDescriptors(createInfo.resourceCounts);

        _maxFramesInFlight = createInfo.maxFramesInFlight;

        for (uint32_t i = 0; i < createInfo.maxFramesInFlight; i++) {
            _deletionQueue.frameQueues.push_back(WilloRHI::DeletionQueues::PerFrame{});
        }

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

        for (uint32_t i = 0; i < _maxFramesInFlight; i++) {
            CollectGarbage();
            NextFrame();
        }

        vkDestroyDescriptorSetLayout(_vkDevice, _globalDescriptors.setLayout, nullptr);
        vkDestroyDescriptorPool(_vkDevice, _globalDescriptors.pool, nullptr);

        for (auto& pool : _commandListPool.commandPools) {
            vkDestroyCommandPool(_vkDevice, pool.second, nullptr);
        }

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

    Swapchain Device::CreateSwapchain(const SwapchainCreateInfo& createInfo) {
        return impl->CreateSwapchain(createInfo); }
    Swapchain ImplDevice::CreateSwapchain(const SwapchainCreateInfo& createInfo)
    {
        LogMessage("Creating swapchain...", false);
        Swapchain newSwapchain;
        newSwapchain.impl = std::make_shared<ImplSwapchain>();
        ImplSwapchain* swapImpl = newSwapchain.impl.get();

        swapImpl->device = this;
        swapImpl->vkDevice = _vkDevice;
        swapImpl->vkPhysicalDevice = _vkPhysicalDevice;

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

    CommandList Device::GetCommandList() { return impl->GetCommandList(); }
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

    BufferId Device::CreateBuffer(const BufferCreateInfo& createInfo) {
        return impl->CreateBuffer(createInfo); }
    BufferId ImplDevice::CreateBuffer(const BufferCreateInfo& createInfo)
    {
        const uint32_t queueIndices[3] = {_graphicsQueueIndex, _computeQueueIndex, _transferQueueIndex};

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
            .pQueueFamilyIndices = queueIndices
        };

        VmaAllocationCreateInfo allocInfo = {

        };

        return {};
    }

    void Device::DestroyBinarySemaphore(BinarySemaphore semaphore) {
        impl->DestroyBinarySemaphore(semaphore); }
    void ImplDevice::DestroyBinarySemaphore(BinarySemaphore semaphore) {
        _deletionQueue.frameQueues.at(_frameNum % _maxFramesInFlight)
            .semaphoreQueue.enqueue(semaphore);
    }

    void Device::DestroyTimelineSemaphore(TimelineSemaphore semaphore) {
        impl->DestroyTimelineSemaphore(semaphore); }
    void ImplDevice::DestroyTimelineSemaphore(TimelineSemaphore semaphore) {
        _deletionQueue.frameQueues.at(_frameNum % _maxFramesInFlight)
            .timelineSemaphoreQueue.enqueue(semaphore);
    }

    void Device::DestroySwapchain(Swapchain swapchain) {
        impl->DestroySwapchain(swapchain); }
    void ImplDevice::DestroySwapchain(Swapchain swapchain)
    {
        _deletionQueue.frameQueues.at(_frameNum % _maxFramesInFlight)
            .swapchainQueue.enqueue(swapchain);

        for (int i = 0; i < swapchain.impl->_framesInFlight; i++) {
            _deletionQueue.frameQueues.at(_frameNum % _maxFramesInFlight)
                .semaphoreQueue.enqueue(swapchain.impl->_imageSync.at(i));
        }
    }

    void Device::NextFrame() { impl->NextFrame(); }
    void ImplDevice::NextFrame()
    {
        _frameNum += 1;
    }

    uint64_t Device::GetFrameNum() { return impl->GetFrameNum(); }
    uint64_t ImplDevice::GetFrameNum()
    {
        return _frameNum;
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

    void Device::QueuePresent(const PresentInfo& presentInfo) {
        impl->QueuePresent(presentInfo); }
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
            presentInfo.swapchain->impl->_needResize = true;
            LogMessage("Present needs resize", false);
        }
    }

    void Device::QueueSubmit(const CommandSubmitInfo& submitInfo) {
        impl->QueueSubmit(submitInfo); }
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

    void Device::CollectGarbage() { impl->CollectGarbage(); }
    void ImplDevice::CollectGarbage()
    {
        int64_t frameIndex = _frameNum % _maxFramesInFlight;
        DeletionQueues::PerFrame& queue = _deletionQueue.frameQueues.at(frameIndex);

        // buffer
        BufferId bufferHandle{};
        while (queue.bufferQueue.try_dequeue(bufferHandle)) {
            BufferResource* rsrc = &_resources.buffers.resources[bufferHandle.id];
            vmaDestroyBuffer(_allocator, rsrc->buffer, rsrc->allocation);
            _resources.buffers.freeSlotQueue.enqueue(bufferHandle.id);
#if WilloRHI_LOGGING_VERBOSE
            LogMessage("Destroyed buffer " + std::to_string(bufferHandle.id), false);
#endif
        }

        // image
        ImageId imageHandle{};
        while (queue.imageQueue.try_dequeue(imageHandle)) {
            ImageResource* rsrc = &_resources.images.resources[imageHandle.id];
            vmaDestroyImage(_allocator, rsrc->image, rsrc->allocation);
            _resources.images.freeSlotQueue.enqueue(imageHandle.id);
#if WilloRHI_LOGGING_VERBOSE
            LogMessage("Destroyed image " + std::to_string(imageHandle.id), false);
#endif
        }

        // image view
        ImageViewId viewHandle{};
        while (queue.imageViewQueue.try_dequeue(viewHandle)) {
            ImageViewResource* rsrc = &_resources.imageViews.resources[viewHandle.id];
            vkDestroyImageView(_vkDevice, rsrc->imageView, nullptr);
            _resources.imageViews.freeSlotQueue.enqueue(viewHandle.id);
#if WilloRHI_LOGGING_VERBOSE
            LogMessage("Destroyed image view " + std::to_string(viewHandle.id), false);
#endif
        }

        // sampler
        SamplerId samplerHandle{};
        while (queue.samplerQueue.try_dequeue(samplerHandle)) {
            SamplerResource* rsrc = &_resources.samplers.resources[samplerHandle.id];
            vkDestroySampler(_vkDevice, rsrc->sampler, nullptr);
            _resources.samplers.freeSlotQueue.enqueue(samplerHandle.id);
#if WilloRHI_LOGGING_VERBOSE
            LogMessage("Destroyed sampler " + std::to_string(samplerHandle.id), false);
#endif
        }

        // semaphores
        BinarySemaphore binSemaphore{};
        while (queue.semaphoreQueue.try_dequeue(binSemaphore)) {
            vkDestroySemaphore(_vkDevice, binSemaphore.impl->vkSemaphore, nullptr);
#if WilloRHI_LOGGING_VERBOSE
            LogMessage("Destroyed binary semaphore", false);
#endif
        }

        TimelineSemaphore timSemaphore{};
        while (queue.timelineSemaphoreQueue.try_dequeue(timSemaphore)) {
            vkDestroySemaphore(_vkDevice, timSemaphore.impl->vkSemaphore, nullptr);
#if WilloRHI_LOGGING_VERBOSE
            LogMessage("Destroyed timeline semaphore", false);
#endif
        }

        // commandlists
        CommandList cmdList;
        while (queue.commandLists.try_dequeue(cmdList)) {
            vkResetCommandBuffer(cmdList.impl->_vkCommandBuffer, 0);
            CommandListPool::MapType* map = &_commandListPool.primaryBuffers;
            map->at(cmdList.impl->_threadId).enqueue(cmdList);
        }

        // swapchains
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
