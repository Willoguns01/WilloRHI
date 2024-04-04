#pragma once 

#include "WilloRHI/Device.hpp"
#include "WilloRHI/Util.hpp"

#include <vulkan/vulkan.h>

#include <vk_mem_alloc.h>
#include <VkBootstrap.h>

#include <concurrentqueue.h>

#include <unordered_map>
#include <thread>

namespace WilloRHI
{
    static const inline VkBufferUsageFlags BUFFER_USAGE_FLAGS = 
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

    struct BufferResource {
        VkBuffer buffer = VK_NULL_HANDLE;
        // we wont be using BDA for now until HLSL has better support
        VkDeviceAddress deviceAddress = 0;
        VmaAllocation allocation = VK_NULL_HANDLE;
        BufferCreateInfo createInfo = {};
    };

    struct ImageResource {
        VkImage image = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        ImageCreateInfo createInfo = {};
    };

    struct ImageViewResource {
        VkImageView imageView = VK_NULL_HANDLE;
        ImageViewCreateInfo createInfo = {};
    };

    struct SamplerResource {
        VkSampler sampler = VK_NULL_HANDLE;
    };

    struct DeletionQueues
    {
        struct PerFrame {
            moodycamel::ConcurrentQueue<BufferId> bufferQueue = moodycamel::ConcurrentQueue<BufferId>();
            moodycamel::ConcurrentQueue<ImageId> imageQueue = moodycamel::ConcurrentQueue<ImageId>();
            moodycamel::ConcurrentQueue<ImageViewId> imageViewQueue = moodycamel::ConcurrentQueue<ImageViewId>();
            moodycamel::ConcurrentQueue<SamplerId> samplerQueue = moodycamel::ConcurrentQueue<SamplerId>();
            moodycamel::ConcurrentQueue<BinarySemaphore> semaphoreQueue = moodycamel::ConcurrentQueue<BinarySemaphore>();
            moodycamel::ConcurrentQueue<TimelineSemaphore> timelineSemaphoreQueue = moodycamel::ConcurrentQueue<TimelineSemaphore>();
            
            moodycamel::ConcurrentQueue<Swapchain> swapchainQueue = moodycamel::ConcurrentQueue<Swapchain>();

            moodycamel::ConcurrentQueue<CommandList> commandLists = moodycamel::ConcurrentQueue<CommandList>();
        };
        
        std::vector<PerFrame> frameQueues = std::vector<PerFrame>(1);
    };

    template <typename Resource_T>
    struct ResourceMap {
        Resource_T* resources = nullptr;
        moodycamel::ConcurrentQueue<uint64_t> freeSlotQueue;
        uint64_t maxNumResources = 0;

        ResourceMap() {}
        ResourceMap(uint64_t maxCount) {
            resources = new Resource_T[maxCount];
            freeSlotQueue = moodycamel::ConcurrentQueue<uint64_t>(maxCount);
            maxNumResources = maxCount;

            std::vector<uint64_t> vec;
            for (uint64_t i = 0; i < maxNumResources; i++)
                vec.push_back(i);
            freeSlotQueue.enqueue_bulk(vec.begin(), maxNumResources);
        }
    };

    struct DeviceResources {
        ResourceMap<BufferResource> buffers;
        ResourceMap<ImageResource> images;
        ResourceMap<ImageViewResource> imageViews;
        ResourceMap<SamplerResource> samplers;
    };

    struct CommandListPool {
        std::unordered_map<std::thread::id, VkCommandPool> commandPools;
        typedef std::unordered_map<std::thread::id, moodycamel::ConcurrentQueue<CommandList>> MapType;
        MapType primaryBuffers;
    };

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

        VkQueue _graphicsQueue = VK_NULL_HANDLE;
        uint32_t _graphicsQueueIndex = 0;
        VkQueue _computeQueue = VK_NULL_HANDLE;
        uint32_t _computeQueueIndex = 0;
        VkQueue _transferQueue = VK_NULL_HANDLE;
        uint32_t _transferQueueIndex = 0;

        DeviceResources _resources;
        DeletionQueues _deletionQueue;
        GlobalDescriptors _globalDescriptors;
        CommandListPool _commandListPool;

        RHILoggingFunc _loggingCallback = nullptr;
        bool _doLogInfo = false;

        uint32_t _maxFramesInFlight = 0;
        uint64_t _frameNum = 0;

        // functionality

        void Init(const DeviceCreateInfo& createInfo);

        void SetupQueues(vkb::Device vkbDevice);
        void SetupDescriptors(const ResourceCountInfo& countInfo);
        void SetupDefaultResources();

        void Cleanup();
        void WaitIdle() const;

        // creation functions

        BinarySemaphore CreateBinarySemaphore();
        TimelineSemaphore CreateTimelineSemaphore(uint64_t initialValue);
        Swapchain CreateSwapchain(const SwapchainCreateInfo& createInfo);

        CommandList GetCommandList();

        BufferId CreateBuffer(const BufferCreateInfo& createInfo);

        // deletion functions

        void DestroyBinarySemaphore(BinarySemaphore semaphore);
        void DestroyTimelineSemaphore(TimelineSemaphore semaphore);
        void DestroySwapchain(Swapchain swapchain);

        // functionality

        void NextFrame();
        uint64_t GetFrameNum();

        void WaitSemaphore(TimelineSemaphore semaphore, uint64_t value, uint64_t timeout);
        uint64_t GetSemaphoreValue(TimelineSemaphore semaphore);

        void QueuePresent(const PresentInfo& presentInfo);
        void QueueSubmit(const CommandSubmitInfo& submitInfo);

        // call at the beginning of each frame - finalises destruction of 
        // any resources set as destroyed for that frame
        void CollectGarbage();

        void LogMessage(const std::string& message, bool error = true);
        void ErrorCheck(uint64_t errorCode);

        VkSurfaceKHR CreateSurface(NativeWindowHandle handle);

        // gets next available slot for resource, adds into maps
        // will also do other things like write descriptor array
        ImageId AddImageResource(VkImage image, VmaAllocation allocation, const ImageCreateInfo& createInfo);
        ImageViewId AddImageViewResource(VkImageView view, const ImageViewCreateInfo& createInfo);
    };
}
