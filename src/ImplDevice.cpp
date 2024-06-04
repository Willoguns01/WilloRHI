#include "ImplDevice.hpp"

#include "WilloRHI/WilloRHI_Shared.h"

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
            .drawIndirectCount = true,
            .descriptorIndexing = true,
            .descriptorBindingSampledImageUpdateAfterBind = true,
            .descriptorBindingStorageImageUpdateAfterBind = true,
            .descriptorBindingStorageBufferUpdateAfterBind = true,
            .runtimeDescriptorArray = true,
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

        _vkQueueIndices[0] = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
        _vkQueueIndices[1] = vkbDevice.get_queue_index(vkb::QueueType::compute).value();
        _vkQueueIndices[2] = vkbDevice.get_queue_index(vkb::QueueType::transfer).value();

        SetupDescriptors(createInfo.resourceCounts);

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

        vmaDestroyBuffer(_allocator, _addressBuffer.buffer, _addressBuffer.allocation);

        vmaDestroyAllocator(_allocator);
        vkDestroyDevice(_vkDevice, nullptr);
        LogMessage("Destroy device", false);

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
                .binding = WilloRHI_STORAGE_BUFFER_BINDING,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = (uint32_t)countInfo.bufferCount,
                .stageFlags = VK_SHADER_STAGE_ALL,
                .pImmutableSamplers = nullptr
            },
            VkDescriptorSetLayoutBinding {
                .binding = WilloRHI_STORAGE_IMAGE_BINDING,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .descriptorCount = (uint32_t)countInfo.imageCount,
                .stageFlags = VK_SHADER_STAGE_ALL,
                .pImmutableSamplers = nullptr
            },
            VkDescriptorSetLayoutBinding {
                .binding = WilloRHI_SAMPLED_IMAGE_BINDING,
                .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                .descriptorCount = (uint32_t)countInfo.imageCount,
                .stageFlags = VK_SHADER_STAGE_ALL,
                .pImmutableSamplers = nullptr
            },
            VkDescriptorSetLayoutBinding {
                .binding = WilloRHI_SAMPLER_BINDING,
                .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
                .descriptorCount = (uint32_t)countInfo.samplerCount,
                .stageFlags = VK_SHADER_STAGE_ALL,
                .pImmutableSamplers = nullptr
            },
            VkDescriptorSetLayoutBinding {
                .binding = WilloRHI_DEVICE_ADDRESS_BUFFER_BINDING,
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
            .usage = usageFlags,
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
        _addressBufferPtr = (uint64_t*)_addressBuffer.mappedAddress;

        VkDescriptorBufferInfo bufferDescriptorInfo = {
            .buffer = _addressBuffer.buffer,
            .offset = 0,
            .range = (sizeof(uint64_t) * countInfo.bufferCount)
        };

        VkWriteDescriptorSet descriptorWriteDesc = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = _globalDescriptors.descriptorSet,
            .dstBinding = WilloRHI_DEVICE_ADDRESS_BUFFER_BINDING,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pImageInfo = nullptr,
            .pBufferInfo = &bufferDescriptorInfo,
            .pTexelBufferView = nullptr
        };

        vkUpdateDescriptorSets(_vkDevice, 1, &descriptorWriteDesc, 0, nullptr);

        LogMessage("Created buffer-address buffer for address count of " + std::to_string(countInfo.bufferCount), false);
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

        uint32_t bufferSlot = _resources.buffers.Allocate();

        // create and allocate for buffer

        VkBufferCreateInfo vkBufferInfo = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .size = createInfo.size,
            .usage = BUFFER_USAGE_FLAGS,
            .sharingMode = VK_SHARING_MODE_CONCURRENT,
            .queueFamilyIndexCount = 3,
            .pQueueFamilyIndices = _vkQueueIndices
        };

        VmaAllocationCreateFlags allocFlags = static_cast<VmaAllocationCreateFlags>(createInfo.allocationFlags);
        if (createInfo.allocationFlags & AllocationUsageFlag::HOST_ACCESS_SEQUENTIAL_WRITE ||
            createInfo.allocationFlags & AllocationUsageFlag::HOST_ACCESS_RANDOM) {
                allocFlags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
                newBuffer.isMapped = true;
            }

        VmaAllocationCreateInfo allocationCreateInfo = {
            .flags = allocFlags,
            .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
            .requiredFlags = {},
            .preferredFlags = {},
            .memoryTypeBits = std::numeric_limits<uint32_t>::max(),
            .pool = nullptr,
            .pUserData = nullptr,
            .priority = 0.5f
        };

        VmaAllocationInfo newAllocation = {};

        ErrorCheck(vmaCreateBuffer(_allocator, &vkBufferInfo, &allocationCreateInfo,
            &newBuffer.buffer, &newBuffer.allocation, &newAllocation));

        newBuffer.mappedAddress = newAllocation.pMappedData;
        newBuffer.createInfo = createInfo;

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
            .dstBinding = WilloRHI_STORAGE_BUFFER_BINDING,
            .dstArrayElement = (uint32_t)bufferSlot,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pImageInfo = nullptr,
            .pBufferInfo = &bufferDescriptorInfo,
            .pTexelBufferView = nullptr
        };

        vkUpdateDescriptorSets(_vkDevice, 1, &descriptorWriteDesc, 0, nullptr);

        _resources.buffers.At(bufferSlot) = newBuffer;

        return bufferSlot;
    }

    ImageId Device::CreateImage(const ImageCreateInfo& createInfo) {
        return impl->CreateImage(createInfo); }
    ImageId ImplDevice::CreateImage(const ImageCreateInfo& createInfo)
    {
        ImageResource newImage = {};

        uint32_t imageSlot = _resources.images.Allocate();

        VkImageType imageTypeDims[3] = {VK_IMAGE_TYPE_1D, VK_IMAGE_TYPE_2D, VK_IMAGE_TYPE_3D};
        VkImageType imageType = imageTypeDims[createInfo.dimensions - 1];

        VkImageCreateInfo vkImageInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = static_cast<VkImageCreateFlags>(createInfo.createFlags),
            .imageType = imageType,
            .format = static_cast<VkFormat>(createInfo.format),
            .extent = VkExtent3D{createInfo.size.width, createInfo.size.height, createInfo.size.depth},
            .mipLevels = createInfo.numLevels,
            .arrayLayers = createInfo.numLayers,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = static_cast<VkImageTiling>(createInfo.tiling),
            .usage = static_cast<VkImageUsageFlags>(createInfo.usageFlags),
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };

        VmaAllocationCreateFlags allocFlags = static_cast<VmaAllocationCreateFlags>(createInfo.allocationFlags);
        if (createInfo.allocationFlags & AllocationUsageFlag::HOST_ACCESS_SEQUENTIAL_WRITE ||
            createInfo.allocationFlags & AllocationUsageFlag::HOST_ACCESS_RANDOM) {
                allocFlags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
                newImage.isMapped = true;
            }

        VmaAllocationCreateInfo allocationCreateInfo = {
            .flags = allocFlags,
            .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
            .requiredFlags = {},
            .preferredFlags = {},
            .memoryTypeBits = std::numeric_limits<uint32_t>::max(),
            .pool = nullptr,
            .pUserData = nullptr,
            .priority = 0.5f
        };

        VmaAllocationInfo newAllocation = {};

        ErrorCheck(vmaCreateImage(_allocator, &vkImageInfo, &allocationCreateInfo,
            &newImage.image, &newImage.allocation, &newAllocation));

        newImage.mappedAddress = newAllocation.pMappedData;
        newImage.createInfo = createInfo;
        newImage.aspect = AspectFromFormat(createInfo.format);

        _resources.images.At(imageSlot) = newImage;

        // no descriptor writes here! that's up to image views

        return imageSlot;
    }

    ImageViewId Device::CreateImageView(const ImageViewCreateInfo& createInfo) {
        return impl->CreateImageView(createInfo); }
    ImageViewId ImplDevice::CreateImageView(const ImageViewCreateInfo& createInfo)
    {
        ImageResource& imageRsrc = _resources.images.At(createInfo.image);
        uint32_t viewSlot = _resources.imageViews.Allocate();
        ImageViewResource newImageView = {};
        newImageView.createInfo = createInfo;

        VkImageViewCreateInfo vkImageViewInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = imageRsrc.image,
            .viewType = static_cast<VkImageViewType>(createInfo.viewType),
            .format = static_cast<VkFormat>(createInfo.format),
            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY
            },
            .subresourceRange = {
                .aspectMask = AspectFromFormat(createInfo.format),
                .baseMipLevel = createInfo.subresource.baseLevel,
                .levelCount = createInfo.subresource.numLevels,
                .baseArrayLayer = createInfo.subresource.baseLayer,
                .layerCount = createInfo.subresource.numLayers
            }
        };

        ErrorCheck(vkCreateImageView(_vkDevice, &vkImageViewInfo, nullptr, &newImageView.imageView));

        _resources.imageViews.At(viewSlot) = newImageView;

        std::vector<VkWriteDescriptorSet> descWrites;
        descWrites.reserve(2);

        if (imageRsrc.createInfo.usageFlags & ImageUsageFlag::STORAGE) {
            LogMessage("Create storage view", false);
            VkDescriptorImageInfo storageImageDescriptor = {
                .sampler = VK_NULL_HANDLE,
                .imageView = newImageView.imageView,
                .imageLayout = VK_IMAGE_LAYOUT_GENERAL
            };

            descWrites.push_back({
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = nullptr,
                .dstSet = _globalDescriptors.descriptorSet,
                .dstBinding = WilloRHI_STORAGE_IMAGE_BINDING,
                .dstArrayElement = (uint32_t)viewSlot,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .pImageInfo = &storageImageDescriptor,
                .pBufferInfo = nullptr,
                .pTexelBufferView = nullptr
            });
        }

        if (imageRsrc.createInfo.usageFlags & ImageUsageFlag::SAMPLED) {
            LogMessage("Create sampling view", false);
            VkDescriptorImageInfo sampledImageDescriptor = {
                .sampler = VK_NULL_HANDLE,
                .imageView = newImageView.imageView,
                .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL
            };

            descWrites.push_back({
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = nullptr,
                .dstSet = _globalDescriptors.descriptorSet,
                .dstBinding = WilloRHI_SAMPLED_IMAGE_BINDING,
                .dstArrayElement = (uint32_t)viewSlot,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                .pImageInfo = &sampledImageDescriptor,
                .pBufferInfo = nullptr,
                .pTexelBufferView = nullptr
            });
        }

        vkUpdateDescriptorSets(_vkDevice, (uint32_t)descWrites.size(), descWrites.data(), 0, nullptr);

        return viewSlot;
    }

    SamplerId Device::CreateSampler(const SamplerCreateInfo& createInfo) {
        return impl->CreateSampler(createInfo); }
    SamplerId ImplDevice::CreateSampler(const SamplerCreateInfo& createInfo)
    {
        SamplerResource newSampler = {};
        uint32_t samplerSlot = _resources.samplers.Allocate();
        newSampler.createInfo = createInfo;

        VkSamplerReductionModeCreateInfo vkReductionMode = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO,
            .pNext = nullptr,
            .reductionMode = static_cast<VkSamplerReductionMode>(createInfo.reductionMode)
        };

        VkSamplerCreateInfo vkSamplerInfo = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .magFilter = static_cast<VkFilter>(createInfo.magFilter),
            .minFilter = static_cast<VkFilter>(createInfo.minFilter),
            .mipmapMode = static_cast<VkSamplerMipmapMode>(createInfo.mipFilter),
            .addressModeU = static_cast<VkSamplerAddressMode>(createInfo.addressModeU),
            .addressModeV = static_cast<VkSamplerAddressMode>(createInfo.addressModeV),
            .addressModeW = static_cast<VkSamplerAddressMode>(createInfo.addressModeW),
            .mipLodBias = createInfo.lodBias,
            .anisotropyEnable = createInfo.anisotropyEnable,
            .maxAnisotropy = createInfo.maxAnisotropy,
            .compareEnable = createInfo.compareOpEnable,
            .compareOp = static_cast<VkCompareOp>(createInfo.compareOp),
            .minLod = createInfo.minLod,
            .maxLod = createInfo.maxLod,
            .borderColor = static_cast<VkBorderColor>(createInfo.borderColour),
            .unnormalizedCoordinates = createInfo.unnormalizedCoordinates
        };

        ErrorCheck(vkCreateSampler(_vkDevice, &vkSamplerInfo, nullptr, &newSampler.sampler));

        _resources.samplers.At(samplerSlot) = newSampler;

        VkDescriptorImageInfo samplerDescriptor = {
            .sampler = newSampler.sampler,
            .imageView = nullptr,
            .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };

        VkWriteDescriptorSet descriptorWrite = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = _globalDescriptors.descriptorSet,
            .dstBinding = WilloRHI_SAMPLER_BINDING,
            .dstArrayElement = (uint32_t)samplerSlot,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
            .pImageInfo = &samplerDescriptor,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        };

        vkUpdateDescriptorSets(_vkDevice, 1, &descriptorWrite, 0, nullptr);

        return samplerSlot;
    }

    void* Device::GetBufferPointer(BufferId buffer) { return impl->GetBufferPointer(buffer); }
    void* ImplDevice::GetBufferPointer(BufferId buffer) {
        BufferResource& rsrc = _resources.buffers.At(buffer);
        if (!rsrc.isMapped) {
            LogMessage("Buffer " + std::to_string(buffer) + " is not mapped");
            return nullptr;
        }
        return rsrc.mappedAddress;
    }

    void* Device::GetImagePointer(ImageId image) { return impl->GetImagePointer(image); }
    void* ImplDevice::GetImagePointer(ImageId image) {
        ImageResource& rsrc = _resources.images.At(image);
        if (!rsrc.isMapped) {
            LogMessage("Image " + std::to_string(image) + " is not mapped");
            return nullptr;
        }
        return rsrc.mappedAddress;
    }

    void Device::DestroyBuffer(BufferId buffer) { impl->DestroyBuffer(buffer); }
    void ImplDevice::DestroyBuffer(BufferId buffer) {
        BufferResource& rsrc = _resources.buffers.At(buffer);
        vmaDestroyBuffer(_allocator, rsrc.buffer, rsrc.allocation);
        _resources.buffers.Free(buffer);
    }

    void Device::DestroyImage(ImageId image) { impl->DestroyImage(image); }
    void ImplDevice::DestroyImage(ImageId image) {
        ImageResource& rsrc = _resources.images.At(image);
        vmaDestroyImage(_allocator, rsrc.image, rsrc.allocation);
        _resources.images.Free(image);
    }

    void Device::DestroyImageView(ImageViewId imageView) { impl->DestroyImageView(imageView); }
    void ImplDevice::DestroyImageView(ImageViewId imageView) {
        ImageViewResource& rsrc = _resources.imageViews.At(imageView);
        vkDestroyImageView(_vkDevice, rsrc.imageView, nullptr);
        _resources.imageViews.Free(imageView);
    }

    void Device::DestroySampler(SamplerId sampler) { impl->DestroySampler(sampler); }
    void ImplDevice::DestroySampler(SamplerId sampler) {
        SamplerResource& rsrc = _resources.samplers.At(sampler);
        vkDestroySampler(_vkDevice, rsrc.sampler, nullptr);
        _resources.samplers.Free(sampler);
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
        return static_cast<void*>(_resources.buffers.At(handle).buffer);
    }
    
    void* Device::GetImageNativeHandle(ImageId handle) const { return impl->GetImageNativeHandle(handle); }
    void* ImplDevice::GetImageNativeHandle(ImageId handle) const {
        return static_cast<void*>(_resources.images.At(handle).image);
    }

    void* Device::GetImageViewNativeHandle(ImageViewId handle) const { return impl->GetImageViewNativeHandle(handle); }
    void* ImplDevice::GetImageViewNativeHandle(ImageViewId handle) const {
        return static_cast<void*>(_resources.imageViews.At(handle).imageView);
    }
    
    void* Device::GetSamplerNativeHandle(SamplerId handle) const { return impl->GetSamplerNativeHandle(handle); }
    void* ImplDevice::GetSamplerNativeHandle(SamplerId handle) const {
        return static_cast<void*>(_resources.samplers.At(handle).sampler);
    }

    void* Device::GetNativeSetLayout() const {return impl->GetNativeSetLayout();}
    void* ImplDevice::GetNativeSetLayout() const {
        return static_cast<void*>(_globalDescriptors.setLayout);
    }

    void* Device::GetDeviceResources() { return impl->GetDeviceResources(); }
    void* ImplDevice::GetDeviceResources() {
        return static_cast<void*>(&_resources);
    }

    void* Device::GetResourceDescriptors() {
        return impl->GetResourceDescriptors(); }
    void* ImplDevice::GetResourceDescriptors() {
        return static_cast<void*>(&_globalDescriptors);
    }

    void* Device::GetAllocator() const { return impl->GetAllocator(); }
    void* ImplDevice::GetAllocator() const {
        return static_cast<void*>(_allocator);
    }
}
