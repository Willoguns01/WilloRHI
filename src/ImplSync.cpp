#include "ImplSync.hpp"

namespace WilloRHI
{
    BinarySemaphore BinarySemaphore::Create(Device device)
    {
        BinarySemaphore newSemaphore;
        newSemaphore.impl = std::make_shared<ImplBinarySemaphore>();
        newSemaphore.impl->Init(device);
        return newSemaphore;
    }

    void* BinarySemaphore::GetNativeHandle() const {
        return static_cast<void*>(impl->vkSemaphore);
    }

    void ImplBinarySemaphore::Init(Device device)
    {
        m_Device = device;
        m_vkDevice = static_cast<VkDevice>(m_Device.GetDeviceNativeHandle());

        VkSemaphoreCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0
        };

        m_Device.ErrorCheck(vkCreateSemaphore(m_vkDevice, &createInfo, nullptr, &vkSemaphore));
        m_Device.LogMessage("Created binary semaphore", false);
    }

    ImplBinarySemaphore::~ImplBinarySemaphore()
    {
        vkDestroySemaphore(m_vkDevice, vkSemaphore, nullptr);
    }

    TimelineSemaphore TimelineSemaphore::Create(Device device, uint64_t value)
    {
        TimelineSemaphore newSemaphore;
        newSemaphore.impl = std::make_shared<ImplTimelineSemaphore>();
        newSemaphore.impl->Init(device, value);
        return newSemaphore;
    }

    void* TimelineSemaphore::GetNativeHandle() const {
        return static_cast<void*>(impl->vkSemaphore);
    }

    void ImplTimelineSemaphore::Init(Device device, uint64_t value)
    {
        m_Device = device;
        m_vkDevice = static_cast<VkDevice>(device.GetDeviceNativeHandle());

        VkSemaphoreTypeCreateInfo typeInfo = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
            .pNext = nullptr,
            .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
            .initialValue = value
        };

        VkSemaphoreCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = &typeInfo,
            .flags = 0
        };

        m_Device.ErrorCheck(vkCreateSemaphore(m_vkDevice, &createInfo, nullptr, &vkSemaphore));
        m_Device.LogMessage("Create timeline semaphore", false);
    }

    ImplTimelineSemaphore::~ImplTimelineSemaphore()
    {
        vkDestroySemaphore(m_vkDevice, vkSemaphore, nullptr);
    }

    void TimelineSemaphore::WaitValue(uint64_t value, uint64_t timeout) { impl->WaitValue(value, timeout); }
    void ImplTimelineSemaphore::WaitValue(uint64_t value, uint64_t timeout) {
        VkSemaphoreWaitInfo waitInfo = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
            .pNext = nullptr,
            .semaphoreCount = 1,
            .pSemaphores = &vkSemaphore,
            .pValues = &value
        };

        m_Device.ErrorCheck(vkWaitSemaphores(m_vkDevice, &waitInfo, timeout));
    }

    uint64_t TimelineSemaphore::GetValue() { return impl->GetValue(); }
    uint64_t ImplTimelineSemaphore::GetValue() {
        uint64_t result = 0;
        m_Device.ErrorCheck(vkGetSemaphoreCounterValue(m_vkDevice, vkSemaphore, &result));
        return result;
    }
}
