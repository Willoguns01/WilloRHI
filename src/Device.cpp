#include "ImplDevice.hpp"

namespace WilloRHI
{
    Device::Device()
        : impl(new ImplDevice)
        {}

    Device Device::CreateDevice(const DeviceCreateInfo& createInfo) {
        return ImplDevice::CreateDevice(createInfo);
    }

    void Device::Cleanup() {
        impl->Cleanup();
    }

    void Device::WaitIdle() const {
        impl->WaitIdle();
    }
    
    // creation

    BinarySemaphore Device::CreateBinarySemaphore() {
        return impl->CreateBinarySemaphore();
    }

    TimelineSemaphore Device::CreateTimelineSemaphore(uint64_t initialValue) {
        return impl->CreateTimelineSemaphore(initialValue);
    }

    Swapchain Device::CreateSwapchain(const SwapchainCreateInfo& createInfo) {
        return impl->CreateSwapchain(createInfo, this);
    }

    CommandList Device::GetCommandList() {
        return impl->GetCommandList();
    }

    // deletion

    void Device::NextFrame() {
        impl->NextFrame();
    }

    uint64_t Device::GetFrameNum() {
        return impl->GetFrameNum();
    }

    void Device::DestroyBinarySemaphore(BinarySemaphore semaphore) {
        impl->DestroyBinarySemaphore(semaphore);
    }

    void Device::DestroyTimelineSemaphore(TimelineSemaphore semaphore) {
        impl->DestroyTimelineSemaphore(semaphore);
    }

    void Device::DestroySwapchain(Swapchain swapchain) {
        impl->DestroySwapchain(swapchain);
    }

    // functionality

    void Device::WaitSemaphore(TimelineSemaphore semaphore, uint64_t value, uint64_t timeout) {
        impl->WaitSemaphore(semaphore, value, timeout);
    }

    uint64_t Device::GetSemaphoreValue(TimelineSemaphore semaphore) {
        return impl->GetSemaphoreValue(semaphore);
    }

    void Device::QueuePresent(const PresentInfo& presentInfo) {
        impl->QueuePresent(presentInfo);
    }

    void Device::QueueSubmit(const CommandSubmitInfo& submitInfo) {
        impl->QueueSubmit(submitInfo);
    }

    void Device::CollectGarbage() {
        impl->CollectGarbage();
    }
}
