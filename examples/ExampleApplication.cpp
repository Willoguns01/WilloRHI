#include "ExampleApplication.hpp"

#include <iostream>

static void OutputMessage(const std::string& message) {
    std::cout << message.c_str() << "\n";
}

void ExampleApplication::Init(bool vsync, uint32_t frameOverlap, bool debug, uint32_t width, uint32_t height, const std::string& applicationName)
{
    doDebug = debug;
    doVsync = vsync;
    maxFrameOverlap = frameOverlap;
    frameNum = frameOverlap;

    renderingWidth = width;
    renderingHeight = height;

    // window
    if (!glfwInit()) {
        std::cerr << "Failed to init GLFW\n";
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    window = glfwCreateWindow(1280, 720, applicationName.c_str(), nullptr, nullptr);
    if (window == nullptr && doDebug) {
        std::cerr << "Failed to create window\n";
    }

    HWND hwnd = glfwGetWin32Window(window);
    glfwSwapInterval(1);

    // WilloRHI stuff
    WilloRHI::DeviceCreateInfo deviceInfo = {
        .applicationName = applicationName,
        .validationLayers = debug,
        .logCallback = debug ? &OutputMessage : nullptr,
        .logInfo = debug,
        .resourceCounts = {}
    };

    WilloRHI::SwapchainCreateInfo swapchainInfo = {
        .windowHandle = hwnd,
        .format = WilloRHI::Format::B8G8R8A8_UNORM,
        .presentMode = vsync ? WilloRHI::PresentMode::FIFO : WilloRHI::PresentMode::IMMEDIATE,
        .width = width,
        .height = height,
        .framesInFlight = frameOverlap
    };

    device = WilloRHI::Device::CreateDevice(deviceInfo);
    graphicsQueue = WilloRHI::Queue::Create(device, WilloRHI::QueueType::GRAPHICS);
    pipelineManager = WilloRHI::PipelineManager::Create(device);
    
    swapchain = WilloRHI::Swapchain::Create(device, swapchainInfo);

    uint64_t longFrameOverlap = (uint64_t)frameOverlap;
    timelineSemaphore = WilloRHI::TimelineSemaphore::Create(device, longFrameOverlap);
    for (uint32_t i = 0; i < frameOverlap; i++) {
        renderSemaphores.push_back(WilloRHI::BinarySemaphore::Create(device));
    }
}

void ExampleApplication::Run()
{
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        Update();
        
        timelineSemaphore.WaitValue(frameNum - (maxFrameOverlap - 1), 1000000000);
        frameNum += 1;

        WilloRHI::ImageId swapchainImage = swapchain.AcquireNextImage();

        Render(swapchainImage, swapchain.GetAcquireSemaphore(), renderSemaphores.at(frameNum % maxFrameOverlap));

        WilloRHI::PresentInfo presentInfo = {
            .waitSemaphores = { renderSemaphores.at(frameNum % maxFrameOverlap) },
            .swapchain = swapchain
        };

        graphicsQueue.Present(presentInfo);
        graphicsQueue.CollectGarbage();
    }

    device.WaitIdle();
    graphicsQueue.CollectGarbage();

    glfwDestroyWindow(window);
    glfwTerminate();
}

void ExampleApplication::Update()
{
    if (swapchain.NeedsResize()) {
        int newWidth, newHeight;
        glfwGetWindowSize(window, &newWidth, &newHeight);
        device.WaitIdle();
        OnResize((uint32_t)newWidth, (uint32_t)newHeight);
    }
}

void ExampleApplication::OnResize(uint32_t width, uint32_t height)
{
    swapchain.Resize(width, height, doVsync ? WilloRHI::PresentMode::FIFO : WilloRHI::PresentMode::IMMEDIATE);

    renderingWidth = width;
    renderingHeight = height;
}
