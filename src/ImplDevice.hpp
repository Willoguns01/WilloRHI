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
        BufferResource _addressBuffer;
        uint64_t* _addressBufferPtr = nullptr;

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
        ImageId CreateImage(const ImageCreateInfo& createInfo);
        ImageViewId CreateImageView(const ImageViewCreateInfo& createInfo);
        SamplerId CreateSampler(const SamplerCreateInfo& createInfo);

        void* GetBufferPointer(BufferId buffer);
        void* GetImagePointer(ImageId image);

        void DestroyBuffer(BufferId buffer);
        void DestroyImage(ImageId image);
        void DestroyImageView(ImageViewId imageView);
        void DestroySampler(SamplerId sampler);

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

        void* GetNativeSetLayout() const;

        void* GetDeviceResources();
        void* GetResourceDescriptors();
        void* GetAllocator() const;
    };
}
