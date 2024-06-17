#pragma once

#include <WilloRHI/WilloRHI.hpp>

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

class ExampleApplication
{
public:
    virtual void Init(bool vsync, uint32_t frameOverlap, bool debug, uint32_t width, uint32_t height, const std::string& applicationName);
    virtual void Load() = 0;

    void Run();

    virtual void Update();
    virtual void Render(WilloRHI::ImageId swapchainImage, WilloRHI::BinarySemaphore acquireSemaphore, WilloRHI::BinarySemaphore renderSemaphore) = 0;

    virtual void OnResize(uint32_t width, uint32_t height);

    WilloRHI::Device device;
    WilloRHI::Queue graphicsQueue;
    WilloRHI::PipelineManager pipelineManager;

    WilloRHI::TimelineSemaphore timelineSemaphore;
    uint64_t frameNum;

    uint32_t renderingWidth = 0;
    uint32_t renderingHeight = 0;

private:
    WilloRHI::Swapchain swapchain;

    GLFWwindow* window = nullptr;

    bool doDebug = false;
    bool doVsync = false;

    std::vector<WilloRHI::BinarySemaphore> renderSemaphores;
    uint32_t maxFrameOverlap;
};
