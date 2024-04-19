#pragma once 

#include "WilloRHI/Device.hpp"
#include "ImplResources.hpp"

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <VkBootstrap.h>
#include <concurrentqueue.h>

namespace WilloRHI
{
    using NativeWindowHandle = void*;

    struct GlobalDescriptors {
        VkDescriptorPool pool;
        VkDescriptorSetLayout setLayout;
        VkDescriptorSet descriptorSet;
    };
    
    struct ImplDevice
    {
        // vars
        VkInstance _vkInstance = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT _vkDebugMessenger = VK_NULL_HANDLE;
        VkPhysicalDevice _vkPhysicalDevice = VK_NULL_HANDLE;
        VkDevice _vkDevice = VK_NULL_HANDLE;
        vkb::Device _vkbDevice;
        VmaAllocator _allocator = VK_NULL_HANDLE;
        vkb::SystemInfo _sysInfo = vkb::SystemInfo::get_system_info().value();

        uint32_t _vkQueueIndices[3] = {0,0,0};

        DeviceResources _resources;
        GlobalDescriptors _globalDescriptors;

        RHILoggingFunc _loggingCallback = nullptr;
        bool _doLogInfo = false;

        // functionality

        void Init(const DeviceCreateInfo& createInfo);

        ~ImplDevice();
        void Cleanup();

        void SetupDescriptors(const ResourceCountInfo& countInfo);
        void SetupDefaultResources();

        void* GetDeviceNativeHandle() const;
        void* GetPhysicalDeviceNativeHandle() const;
        void* GetInstanceNativeHandle() const;

        void WaitIdle() const;

        // resources

        BufferId CreateBuffer(const BufferCreateInfo& createInfo);

        // functionality

        void LogMessage(const std::string& message, bool error = true);
        void ErrorCheck(uint64_t errorCode);

        void LockResources_Shared();
        void UnlockResources_Shared();
        void LockResources();
        void UnlockResources();

        void* GetBufferNativeHandle(BufferId handle) const;
        void* GetImageNativeHandle(ImageId handle) const;
        void* GetImageViewNativeHandle(ImageViewId handle) const;
        void* GetSamplerNativeHandle(SamplerId handle) const;

        void* GetDeviceResources();
        void* GetAllocator() const;
    };
}
