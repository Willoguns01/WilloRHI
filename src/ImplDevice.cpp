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

    void* Device::GetDeviceNativeHandle() const { return impl->GetDeviceNativeHandle(); }
    void* ImplDevice::GetDeviceNativeHandle() const {
        return static_cast<void*>(_vkDevice);
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
        VkBufferCreateInfo bufferInfo = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .size = 0,
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
