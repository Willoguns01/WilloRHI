#pragma once

#include "WilloRHI/Forward.hpp"
#include "WilloRHI/Resources.hpp"

#include <stdint.h>
#include <string>
#include <memory>

#undef CreateSemaphore

namespace WilloRHI
{
    // these are some sane-ish defaults to allow any application run just fine
    // this exists for optimisation purposes where the client app may want
    // to reduce memory usage or runtime performance costs of having many descriptors
    struct ResourceCountInfo
    {
        uint32_t bufferCount = 1u << 20u;
        uint32_t imageCount = 1u << 20u;
        uint32_t samplerCount = 1u << 20u;
    };

    typedef void(*RHILoggingFunc)(const std::string&);
    struct DeviceCreateInfo
    {
        std::string applicationName = "WilloRHI Application";
        bool validationLayers = false;
        RHILoggingFunc logCallback = nullptr;
        bool logInfo = false;
        ResourceCountInfo resourceCounts = {};
    };

    class Device
    {
    public:
        Device() = default;
        static Device CreateDevice(const DeviceCreateInfo& createInfo);

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

        // immediate resource destruction
        // these are gpu-unsafe, only use when you KNOW a resource is no longer needed by GPU

        void DestroyBuffer(BufferId buffer);
        void DestroyImage(ImageId image);
        void DestroyImageView(ImageViewId imageView);
        void DestroySampler(SamplerId sampler);

        // functionality

        void LogMessage(const std::string& message, bool error = true);
        void ErrorCheck(uint64_t errorCode);

    protected:
        friend ImplSwapchain;
        friend ImplQueue;
        friend ImplCommandList;
        friend ImplPipelineManager;
        std::shared_ptr<ImplDevice> impl = nullptr;

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
