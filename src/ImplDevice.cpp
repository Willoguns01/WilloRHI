#include "ImplDevice.hpp"

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

    ImplDevice::~ImplDevice() {
        Cleanup();
    }

    void ImplDevice::Cleanup()
    {
        vkDestroyDescriptorSetLayout(_vkDevice, _globalDescriptors.setLayout, nullptr);
        vkDestroyDescriptorPool(_vkDevice, _globalDescriptors.pool, nullptr);

        vmaDestroyAllocator(_allocator);
        vkDestroyDevice(_vkDevice, nullptr);

        vkb::destroy_debug_utils_messenger(_vkInstance, _vkDebugMessenger);
        vkDestroyInstance(_vkInstance, nullptr);
    }

    void ImplDevice::SetupDescriptors(const ResourceCountInfo& countInfo)
    {
        _resources.buffers = ResourceMap<BufferResource>(countInfo.bufferCount);
        _resources.images = ResourceMap<ImageResource>(countInfo.imageCount);
        _resources.imageViews = ResourceMap<ImageViewResource>(countInfo.imageCount);
        _resources.samplers = ResourceMap<SamplerResource>(countInfo.samplerCount);

        std::vector<VkDescriptorSetLayoutBinding> bindings = {
            VkDescriptorSetLayoutBinding {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = (uint32_t)countInfo.bufferCount,
                .stageFlags = VK_SHADER_STAGE_ALL,
                .pImmutableSamplers = nullptr
            },
            VkDescriptorSetLayoutBinding {
                .binding = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .descriptorCount = (uint32_t)countInfo.imageCount,
                .stageFlags = VK_SHADER_STAGE_ALL,
                .pImmutableSamplers = nullptr
            },
            VkDescriptorSetLayoutBinding {
                .binding = 2,
                .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                .descriptorCount = (uint32_t)countInfo.imageCount,
                .stageFlags = VK_SHADER_STAGE_ALL,
                .pImmutableSamplers = nullptr
            },
            VkDescriptorSetLayoutBinding {
                .binding = 3,
                .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
                .descriptorCount = (uint32_t)countInfo.samplerCount,
                .stageFlags = VK_SHADER_STAGE_ALL,
                .pImmutableSamplers = nullptr
            },
            VkDescriptorSetLayoutBinding {
                .binding = 4,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_ALL,
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

        LogMessage("Allocated resource descriptor set with the following counts:\n - "
            + std::to_string(countInfo.bufferCount) + " Buffers\n - "
            + std::to_string(countInfo.imageCount) + " Images\n - "
            + std::to_string(countInfo.samplerCount) + " Samplers", false);

        // create the BDA address buffer

        _addressBuffer = {};
        
        VkBufferUsageFlags usageFlags = 
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

        VkBufferCreateInfo bufferInfo = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .size = (sizeof(uint64_t) * countInfo.bufferCount),
            .usage = BUFFER_USAGE_FLAGS,
            .sharingMode = VK_SHARING_MODE_CONCURRENT,
            .queueFamilyIndexCount = 3,
            .pQueueFamilyIndices = _vkQueueIndices
        };

        VmaAllocationCreateFlags allocFlags = 
            VMA_ALLOCATION_CREATE_MAPPED_BIT | 
            VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT | 
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

        VmaAllocationCreateInfo bufferAllocInfo = {
            .flags = allocFlags,
            .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
            .requiredFlags = {},
            .preferredFlags = {},
            .memoryTypeBits = std::numeric_limits<uint32_t>::max(),
            .pool = nullptr,
            .pUserData = nullptr,
            .priority = 0.5f
        };

        VmaAllocationInfo newAllocInfo = {};

        vmaCreateBuffer(_allocator, &bufferInfo, &bufferAllocInfo, &_addressBuffer.buffer, &_addressBuffer.allocation, &newAllocInfo);

        _addressBuffer.isMapped = true;
        _addressBuffer.mappedAddress = newAllocInfo.pMappedData;
    }

    void ImplDevice::SetupDefaultResources()
    {
        // default "error" texture, view, empty buffer, default sampler, etc

        LogMessage("Loaded default resources", false);
    }

    void* Device::GetDeviceNativeHandle() const { return impl->GetDeviceNativeHandle(); }
    void* ImplDevice::GetDeviceNativeHandle() const {
        return static_cast<void*>(_vkDevice);
    }

    void* Device::GetPhysicalDeviceNativeHandle() const { return impl->GetPhysicalDeviceNativeHandle(); }
    void* ImplDevice::GetPhysicalDeviceNativeHandle() const {
        return static_cast<void*>(_vkPhysicalDevice);
    }

    void* Device::GetInstanceNativeHandle() const { return impl->GetInstanceNativeHandle(); }
    void* ImplDevice::GetInstanceNativeHandle() const {
        return static_cast<void*>(_vkInstance);
    }

    void Device::WaitIdle() const { impl->WaitIdle(); }
    void ImplDevice::WaitIdle() const
    {
        vkDeviceWaitIdle(_vkDevice);
    }

    BufferId Device::CreateBuffer(const BufferCreateInfo& createInfo) {
        return impl->CreateBuffer(createInfo); }
    BufferId ImplDevice::CreateBuffer(const BufferCreateInfo& createInfo)
    {
        BufferResource newBuffer = {};
        newBuffer.createInfo = createInfo;

        // create and allocate for buffer

        VkBufferCreateInfo bufferInfo = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .size = createInfo.size,
            .usage = BUFFER_USAGE_FLAGS,
            .sharingMode = VK_SHARING_MODE_CONCURRENT,
            .queueFamilyIndexCount = 3,
            .pQueueFamilyIndices = _vkQueueIndices
        };

        VmaAllocationCreateFlags allocFlags = 0;
        if (createInfo.usage & AllocationUsageFlag::HOST_ACCESS_SEQUENTIAL_WRITE ||
            createInfo.usage & AllocationUsageFlag::HOST_ACCESS_RANDOM) {
                allocFlags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
                newBuffer.isMapped = true;
            }

        VmaAllocationCreateInfo allocInfo = {
            .flags = allocFlags,
            .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
            .requiredFlags = {},
            .preferredFlags = {},
            .memoryTypeBits = std::numeric_limits<uint32_t>::max(),
            .pool = nullptr,
            .pUserData = nullptr,
            .priority = 0.5f
        };

        VmaAllocationInfo newAllocInfo = {};

        ErrorCheck(vmaCreateBuffer(_allocator, &bufferInfo, &allocInfo,
            &newBuffer.buffer, &newBuffer.allocation, &newAllocInfo));

        // get new buffer id slot, add resource to map

        uint64_t bufferSlot = _resources.buffers.Allocate();
        _resources.buffers.resources[bufferSlot] = newBuffer;
        
        // fill-out other structure values - void* pointer and device address

        newBuffer.mappedAddress = newAllocInfo.pMappedData;

        VkBufferDeviceAddressInfo addressInfo = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .pNext = nullptr,
            .buffer = newBuffer.buffer
        };

        newBuffer.deviceAddress = vkGetBufferDeviceAddress(_vkDevice, &addressInfo);
        // write address to address buffer pointer
        _addressBufferPtr[bufferSlot] = newBuffer.deviceAddress;

        // write buffer descriptor to same slot as id

        VkDescriptorBufferInfo bufferDescriptorInfo = {
            .buffer = newBuffer.buffer,
            .offset = 0,
            .range = createInfo.size
        };

        VkWriteDescriptorSet descriptorWriteDesc = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = _globalDescriptors.descriptorSet,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pImageInfo = nullptr,
            .pBufferInfo = &bufferDescriptorInfo,
            .pTexelBufferView = nullptr
        };

        vkUpdateDescriptorSets(_vkDevice, 1, &descriptorWriteDesc, 0, nullptr);

        BufferId newBufferId = {};
        newBufferId.id = bufferSlot;

        return newBufferId;
    }

    ImageId Device::CreateImage(const ImageCreateInfo& createInfo) {
        return impl->CreateImage(createInfo); }
    ImageId ImplDevice::CreateImage(const ImageCreateInfo& createInfo)
    {
        return {};
    }

    ImageViewId Device::CreateImageView(const ImageViewCreateInfo& createInfo) {
        return impl->CreateImageView(createInfo); }
    ImageViewId ImplDevice::CreateImageView(const ImageViewCreateInfo& createInfo)
    {
        return {};
    }

    SamplerId Device::CreateSampler(const SamplerCreateInfo& createInfo) {
        return impl->CreateSampler(createInfo); }
    SamplerId ImplDevice::CreateSampler(const SamplerCreateInfo& createInfo)
    {
        return {};
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

    void Device::LockResources_Shared() { impl->LockResources_Shared(); }
    void ImplDevice::LockResources_Shared() {
        _resources.resourcesMutex.lock_shared();
    }

    void Device::UnlockResources_Shared() { impl->UnlockResources_Shared(); }
    void ImplDevice::UnlockResources_Shared() {
        _resources.resourcesMutex.unlock_shared();
    }

    void Device::LockResources() { impl->LockResources(); }
    void ImplDevice::LockResources() {
        _resources.resourcesMutex.lock();
    }

    void Device::UnlockResources() { impl->UnlockResources(); }
    void ImplDevice::UnlockResources() {
        _resources.resourcesMutex.unlock();
    }

    void* Device::GetBufferNativeHandle(BufferId handle) const { return impl->GetBufferNativeHandle(handle); }
    void* ImplDevice::GetBufferNativeHandle(BufferId handle) const {
        return static_cast<void*>(_resources.buffers.At(handle.id).buffer);
    }
    
    void* Device::GetImageNativeHandle(ImageId handle) const { return impl->GetImageNativeHandle(handle); }
    void* ImplDevice::GetImageNativeHandle(ImageId handle) const {
        return static_cast<void*>(_resources.images.At(handle.id).image);
    }

    void* Device::GetImageViewNativeHandle(ImageViewId handle) const { return impl->GetImageViewNativeHandle(handle); }
    void* ImplDevice::GetImageViewNativeHandle(ImageViewId handle) const {
        return static_cast<void*>(_resources.imageViews.At(handle.id).imageView);
    }
    
    void* Device::GetSamplerNativeHandle(SamplerId handle) const { return impl->GetSamplerNativeHandle(handle); }
    void* ImplDevice::GetSamplerNativeHandle(SamplerId handle) const {
        return static_cast<void*>(_resources.samplers.At(handle.id).sampler);
    }

    void* Device::GetDeviceResources() { return impl->GetDeviceResources(); }
    void* ImplDevice::GetDeviceResources() {
        return static_cast<void*>(&_resources);
    }

    void* Device::GetAllocator() const { return impl->GetAllocator(); }
    void* ImplDevice::GetAllocator() const {
        return static_cast<void*>(_allocator);
    }
}
