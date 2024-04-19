#pragma once

#include <shared_mutex>
#include <vulkan/vulkan.h>

#include <concurrentqueue.h>
#include <vk_mem_alloc.h>

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

        uint64_t Allocate() {
            uint64_t newSlot = 0;
            freeSlotQueue.try_dequeue(newSlot);
            return newSlot;
        }

        Resource_T& At(uint64_t index) const {
            return resources[index];
        }

        void Free(uint64_t index) {
            freeSlotQueue.enqueue(index);
        }
    };

    struct DeviceResources {
        ResourceMap<BufferResource> buffers;
        ResourceMap<ImageResource> images;
        ResourceMap<ImageViewResource> imageViews;
        ResourceMap<SamplerResource> samplers;

        std::shared_mutex resourcesMutex;
    };

    struct DeletionQueues {
        // uint64_t is timeline value that object should be deleted on

        template <typename T>
        using QueueType =  moodycamel::ConcurrentQueue<T>;

        QueueType<BufferId> bufferQueue = QueueType<BufferId>();
        QueueType<ImageId> imageQueue = QueueType<ImageId>();
        QueueType<ImageViewId> imageViewQueue = QueueType<ImageViewId>();
        QueueType<SamplerId> samplerQueue = QueueType<SamplerId>();
    };
}
