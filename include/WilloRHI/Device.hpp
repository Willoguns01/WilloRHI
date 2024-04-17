#pragma once

#include "WilloRHI/Forward.hpp"
#include "WilloRHI/Util.hpp"
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
        uint64_t bufferCount = Util::Power(2, 20);
        uint64_t imageCount = Util::Power(2, 20);
        uint64_t imageViewCount = Util::Power(2, 20);
        uint64_t samplerCount = Util::Power(2, 20);
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

        void WaitIdle() const;

        // resources 

        BufferId CreateBuffer(const BufferCreateInfo& createInfo);

        // functionality

        void LogMessage(const std::string& message, bool error = true);
        void ErrorCheck(uint64_t errorCode);

    protected:
        friend ImplSwapchain;
        friend ImplQueue;
        friend ImplCommandList;
        std::shared_ptr<ImplDevice> impl = nullptr;
    };
}
