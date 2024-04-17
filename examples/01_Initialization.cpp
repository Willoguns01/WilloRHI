#include <iostream>

#include <WilloRHI/WilloRHI.hpp>

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <thread>
#include <chrono>

static void OutputMessage(const std::string& message) {
    std::cout << message.c_str() << "\n";
}

constexpr int FRAME_OVERLAP = 3;

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Test Window", nullptr, nullptr);

    HWND hwnd = glfwGetWin32Window(window);
    glfwSwapInterval(1);

    WilloRHI::ResourceCountInfo countInfo = {
        .bufferCount = 32,
        .imageCount = 32,
        .imageViewCount = 32,
        .samplerCount = 32
    };

    WilloRHI::DeviceCreateInfo deviceInfo = {
        .applicationName = "01_Initialization",
        .validationLayers = true,
        .logCallback = &OutputMessage,
        .logInfo = true,
        .resourceCounts = countInfo
    };

    //WilloRHI::DeviceCreateInfo deviceInfo = {
    //    .applicationName = "01_Initialization",
    //    .validationLayers = false,
    //    .logCallback = nullptr,
    //    .logInfo = false,
    //    .resourceCounts = countInfo
    //};

    WilloRHI::Device device = WilloRHI::Device::CreateDevice(deviceInfo);

    WilloRHI::SwapchainCreateInfo swapchainInfo = {
        .windowHandle = hwnd,
        .format = WilloRHI::Format::B8G8R8A8_UNORM,
        .presentMode = WilloRHI::PresentMode::IMMEDIATE,
        .width = 1280,
        .height = 720,
        .framesInFlight = FRAME_OVERLAP
    };

    WilloRHI::Swapchain swapchain = WilloRHI::Swapchain::Create(device, swapchainInfo);

    std::vector<WilloRHI::BinarySemaphore> renderSemaphores;
    for (int i = 0; i < FRAME_OVERLAP; i++) {
        renderSemaphores.push_back(WilloRHI::BinarySemaphore::Create(device));
    }

    WilloRHI::TimelineSemaphore gpuTimeline = WilloRHI::TimelineSemaphore::Create(device, FRAME_OVERLAP);
    uint64_t frameNum = FRAME_OVERLAP;

    WilloRHI::Queue graphicsQueue = WilloRHI::Queue::Create(device, WilloRHI::QueueType::GRAPHICS);
    
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // this is far from optimal resizing, should read up on smooth resize
        // maybe give Swapchain its own Resize() function
        int newWidth, newHeight;
        glfwGetWindowSize(window, &newWidth, &newHeight);
        if (newWidth != swapchainInfo.width || newHeight != swapchainInfo.height) {
            swapchainInfo.width = newWidth;
            swapchainInfo.height = newHeight;
            swapchain.Resize(newWidth, newHeight, swapchainInfo.presentMode);
            continue;
        }

        gpuTimeline.WaitValue(frameNum - (FRAME_OVERLAP - 1), 1000000000);
        frameNum += 1;
        WilloRHI::ImageId swapchainImage = swapchain.AcquireNextImage();

        WilloRHI::CommandList cmdList = graphicsQueue.GetCmdList();
        cmdList.Begin();

        WilloRHI::ImageMemoryBarrierInfo barrierInfo = {
            .srcStage = WilloRHI::PipelineStage::ALL_COMMANDS_BIT,
            .dstStage = WilloRHI::PipelineStage::CLEAR_BIT,
            .srcAccess = WilloRHI::MemoryAccess::READ,
            .dstAccess = WilloRHI::MemoryAccess::WRITE,
            .srcLayout = WilloRHI::ImageLayout::UNDEFINED,
            .dstLayout = WilloRHI::ImageLayout::GENERAL,
            .subresourceRange = {}
        };
        cmdList.TransitionImageLayout(swapchainImage, barrierInfo);

        float clearColour[] = { 0.1f, 0.1f, 0.1f, 1.0f };
        cmdList.ClearImage(swapchainImage, clearColour, barrierInfo.subresourceRange);

        barrierInfo = {
            .srcStage = WilloRHI::PipelineStage::CLEAR_BIT,
            .dstStage = WilloRHI::PipelineStage::ALL_COMMANDS_BIT,
            .srcAccess = WilloRHI::MemoryAccess::WRITE,
            .dstAccess = WilloRHI::MemoryAccess::READ,
            .srcLayout = WilloRHI::ImageLayout::GENERAL,
            .dstLayout = WilloRHI::ImageLayout::PRESENT_SRC
        };
        cmdList.TransitionImageLayout(swapchainImage, barrierInfo);
        
        cmdList.End();

        WilloRHI::CommandSubmitInfo submitInfo = {
            .signalTimelineSemaphores = { { gpuTimeline, frameNum } },
            .waitBinarySemaphores = { swapchain.GetAcquireSemaphore() },
            .signalBinarySemaphores = { renderSemaphores.at(swapchain.GetCurrentImageIndex()) },
            .commandLists = { cmdList }
        };

        graphicsQueue.Submit(submitInfo);

        WilloRHI::PresentInfo presentInfo = {
            .waitSemaphores = { renderSemaphores.at(swapchain.GetCurrentImageIndex()) },
            .swapchain = swapchain
        };

        graphicsQueue.Present(presentInfo);
        graphicsQueue.CollectGarbage();
    }

    device.WaitIdle();
    graphicsQueue.CollectGarbage();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
