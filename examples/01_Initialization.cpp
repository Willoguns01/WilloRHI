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
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Test Window", nullptr, nullptr);

    HWND hwnd = glfwGetWin32Window(window);
    glfwSwapInterval(0);

    WilloRHI::ResourceCountInfo countInfo = {};
    countInfo.bufferCount = 8;
    countInfo.imageCount = 8;
    countInfo.imageViewCount = 8;
    countInfo.samplerCount = 8;

    WilloRHI::DeviceCreateInfo deviceInfo = {
        .applicationName = "01_Initialization",
        .validationLayers = false,
        .logCallback = &OutputMessage,
        .logInfo = true,
        .resourceCounts = countInfo,
        .maxFramesInFlight = FRAME_OVERLAP
    };

    WilloRHI::Device device = WilloRHI::Device::CreateDevice(deviceInfo);

    WilloRHI::SwapchainCreateInfo swapchainInfo = {
        .windowHandle = hwnd,
        .format = WilloRHI::Format::B8G8R8A8_UNORM,
        .presentMode = WilloRHI::PresentMode::IMMEDIATE,
        .width = 1280,
        .height = 720,
        .framesInFlight = FRAME_OVERLAP
    };

    WilloRHI::Swapchain swapchain = device.CreateSwapchain(swapchainInfo);

    std::vector<WilloRHI::BinarySemaphore> renderSemaphores;
    for (int i = 0; i < FRAME_OVERLAP; i++) {
        renderSemaphores.push_back(device.CreateBinarySemaphore());
    }

    WilloRHI::TimelineSemaphore gpuTimeline = device.CreateTimelineSemaphore(0);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        device.WaitSemaphore(gpuTimeline, device.GetFrameNum(), 1000000000);
        device.NextFrame();
        WilloRHI::ImageId swapchainImage = swapchain.AcquireNextImage();
        device.CollectGarbage();

        WilloRHI::CommandList cmdList = device.GetCommandList();
        cmdList.Begin();

        WilloRHI::ImageMemoryBarrierInfo barrierInfo = {
            .srcStage = WilloRHI::PipelineStage::ALL_COMMANDS_BIT,
            .dstStage = WilloRHI::PipelineStage::ALL_COMMANDS_BIT,
            .srcAccess = WilloRHI::MemoryAccess::WRITE,
            .dstAccess = WilloRHI::MemoryAccess::READ | WilloRHI::MemoryAccess::WRITE,
            .srcLayout = WilloRHI::ImageLayout::UNDEFINED,
            .dstLayout = WilloRHI::ImageLayout::GENERAL,
            .subresourceRange = {
                .baseLevel = 0,
                .numLevels = 1,
                .baseLayer = 0,
                .numLayers = 1
            }
        };

        cmdList.TransitionImageLayout(swapchainImage, barrierInfo);
        float clearColour[] = { 0.1f, 0.1f, 0.1f, 1.0f };
        cmdList.ClearImage(swapchainImage, clearColour, barrierInfo.subresourceRange);

        barrierInfo.srcLayout = WilloRHI::ImageLayout::GENERAL;
        barrierInfo.dstLayout = WilloRHI::ImageLayout::PRESENT_SRC;
        cmdList.TransitionImageLayout(swapchainImage, barrierInfo);
        
        cmdList.End();

        WilloRHI::CommandSubmitInfo submitInfo = {
            .queueType = WilloRHI::SubmitQueueType::GRAPHICS,
            .signalTimelineSemaphores = { { gpuTimeline, device.GetFrameNum() } },
            .waitBinarySemaphores = { swapchain.GetAcquireSemaphore() },
            .signalBinarySemaphores = { renderSemaphores.at(swapchain.GetCurrentImageIndex()) },
            .commandLists = { cmdList }
        };

        device.QueueSubmit(submitInfo);

        WilloRHI::PresentInfo presentInfo = {
            .waitSemaphores = { renderSemaphores.at(swapchain.GetCurrentImageIndex()) },
            .swapchain = &swapchain
        };

        device.QueuePresent(presentInfo);
    }

    for (int i = 0; i < FRAME_OVERLAP; i++) {
        device.DestroyBinarySemaphore(renderSemaphores.at(i));
    }
    device.DestroyTimelineSemaphore(gpuTimeline);

    device.DestroySwapchain(swapchain);
    device.Cleanup();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
