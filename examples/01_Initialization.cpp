#include <iostream>
#include <fstream>

#include <WilloRHI/WilloRHI.hpp>

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

static void OutputMessage(const std::string& message) {
    std::cout << message.c_str() << "\n";
}

constexpr int FRAME_OVERLAP = 3;

struct FrameResource {
    WilloRHI::BinarySemaphore renderSemaphore;
    FrameResource(WilloRHI::Device device) {
        renderSemaphore = WilloRHI::BinarySemaphore::Create(device);
    }
};

struct RenderingResources {
    std::vector<FrameResource> frames;
    WilloRHI::TimelineSemaphore timelineSemaphore;
    uint64_t frameNum = FRAME_OVERLAP;

    RenderingResources(WilloRHI::Device device) {
        timelineSemaphore = WilloRHI::TimelineSemaphore::Create(device, FRAME_OVERLAP);
        for (uint64_t i = 0; i < frameNum; i++) {
            frames.push_back(FrameResource(device));
        }
    }
};

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Test Window", nullptr, nullptr);

    HWND hwnd = glfwGetWin32Window(window);
    glfwSwapInterval(1);

    // just roll with the defaults
    WilloRHI::ResourceCountInfo countInfo = {};

    //WilloRHI::DeviceCreateInfo deviceInfo = {
    //    .applicationName = "01_Initialization",
    //    .validationLayers = true,
    //    .logCallback = &OutputMessage,
    //    .logInfo = true,
    //    .resourceCounts = countInfo
    //};

    WilloRHI::DeviceCreateInfo deviceInfo = {
        .applicationName = "01_Initialization",
        .validationLayers = false,
        .logCallback = nullptr,
        .logInfo = false,
        .resourceCounts = countInfo
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

    WilloRHI::Swapchain swapchain = WilloRHI::Swapchain::Create(device, swapchainInfo);
    WilloRHI::Queue graphicsQueue = WilloRHI::Queue::Create(device, WilloRHI::QueueType::GRAPHICS);

    RenderingResources renderingResources(device);

    WilloRHI::PipelineManager pipelineManager = WilloRHI::PipelineManager::Create(device);

    // rendering destination
    WilloRHI::ImageCreateInfo renderImageInfo = {
        .dimensions = 2,
        .size = {1280, 720, 1},
        .numLevels = 1,
        .numLayers = 1,
        .format = WilloRHI::Format::R16G16B16A16_SFLOAT,
        .usageFlags = WilloRHI::ImageUsageFlag::STORAGE | WilloRHI::ImageUsageFlag::TRANSFER_SRC,
        .createFlags = {},
        .allocationFlags = {},
        .tiling = WilloRHI::ImageTiling::OPTIMAL
    };

    WilloRHI::ImageId renderingImage = device.CreateImage(renderImageInfo);

    WilloRHI::ImageViewCreateInfo renderImageViewInfo = {
        .image = renderingImage,
        .viewType = WilloRHI::ImageViewType::VIEW_TYPE_2D,
        .format = WilloRHI::Format::R16G16B16A16_SFLOAT,
        .subresource = {}
    };

    WilloRHI::ImageViewId renderingImageView = device.CreateImageView(renderImageViewInfo);

    // compute shader

    std::ifstream file("resources/shaders/test_compute.spv", std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        std::cout << "Failed to open shader file\n";
    }
    size_t fileSize = (size_t)file.tellg();
    std::vector<uint32_t> spirvBuffer;
    spirvBuffer.resize(fileSize / sizeof(uint32_t));
    file.seekg(0);
    file.read((char*)spirvBuffer.data(), fileSize);
    file.close();

    WilloRHI::ShaderModuleInfo moduleInfo = {
        .byteCode = spirvBuffer.data(),
        .codeSize = (uint32_t)fileSize,// * sizeof(uint32_t),
        .name = "shaders/test_compute.spv"
    };

    WilloRHI::ShaderModule shaderModule = pipelineManager.CompileModule(moduleInfo);

    struct ComputePushConstants {
        uint32_t targetTextureIndex;
    };

    WilloRHI::ComputePipelineInfo pipelineInfo = {
        .module = shaderModule,
        .entryPoint = "main",
        .pushConstantSize = sizeof(ComputePushConstants),
        .name = "Testing Compute Shader"
    };

    WilloRHI::ComputePipeline testComputePipeline = pipelineManager.CompileComputePipeline(pipelineInfo);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // this is far from optimal resizing, should read up on smooth resize
        int windowWidth, windowHeight;
        glfwGetWindowSize(window, &windowWidth, &windowHeight);
        if (windowWidth != swapchainInfo.width || windowHeight != swapchainInfo.height) {
            swapchainInfo.width = windowWidth;
            swapchainInfo.height = windowHeight;
            swapchain.Resize(windowWidth, windowHeight, swapchainInfo.presentMode);
            continue;
        }

        renderingResources.timelineSemaphore.WaitValue(renderingResources.frameNum - (FRAME_OVERLAP - 1), 1000000000);
        renderingResources.frameNum += 1;

        WilloRHI::ImageId swapchainImage = swapchain.AcquireNextImage();

        WilloRHI::CommandList cmdList = graphicsQueue.GetCmdList();
        cmdList.Begin();

        WilloRHI::ImageMemoryBarrierInfo barrierInfo = {
            .dstStage = WilloRHI::PipelineStageFlag::COMPUTE_SHADER,
            .dstAccess = WilloRHI::MemoryAccessFlag::WRITE,
            .dstLayout = WilloRHI::ImageLayout::GENERAL,
            .subresourceRange = {}
        };
        cmdList.ImageMemoryBarrier(renderingImage, barrierInfo);

        ComputePushConstants testConstants = {
            .targetTextureIndex = renderingImageView
        };

        cmdList.BindComputePipeline(testComputePipeline);
        cmdList.PushConstants(0, sizeof(ComputePushConstants), (void*)&testConstants);
        cmdList.Dispatch((uint32_t)std::ceil(renderImageInfo.size.width / 16), (uint32_t)std::ceil(renderImageInfo.size.height / 16), 1);

        barrierInfo = {
            .dstStage = WilloRHI::PipelineStageFlag::TRANSFER,
            .dstAccess = WilloRHI::MemoryAccessFlag::READ,
            .dstLayout = WilloRHI::ImageLayout::TRANSFER_SRC
        };
        cmdList.ImageMemoryBarrier(renderingImage, barrierInfo);

        barrierInfo = {
            .dstStage = WilloRHI::PipelineStageFlag::TRANSFER,
            .dstAccess = WilloRHI::MemoryAccessFlag::WRITE,
            .dstLayout = WilloRHI::ImageLayout::TRANSFER_DST
        };
        cmdList.ImageMemoryBarrier(swapchainImage, barrierInfo);

        cmdList.BlitImage(renderingImage, swapchainImage, WilloRHI::Filter::LINEAR);

        barrierInfo = {
            .dstStage = WilloRHI::PipelineStageFlag::ALL_GRAPHICS,
            .dstAccess = WilloRHI::MemoryAccessFlag::READ,
            .dstLayout = WilloRHI::ImageLayout::PRESENT_SRC
        };
        cmdList.ImageMemoryBarrier(swapchainImage, barrierInfo);

        cmdList.End();

        WilloRHI::CommandSubmitInfo submitInfo = {
            .signalTimelineSemaphores = { { renderingResources.timelineSemaphore, renderingResources.frameNum } },
            .waitBinarySemaphores = { swapchain.GetAcquireSemaphore() },
            .signalBinarySemaphores = { renderingResources.frames.at(renderingResources.frameNum % FRAME_OVERLAP).renderSemaphore },
            .commandLists = { cmdList }
        };

        graphicsQueue.Submit(submitInfo);

        WilloRHI::PresentInfo presentInfo = {
            .waitSemaphores = { renderingResources.frames.at(renderingResources.frameNum % FRAME_OVERLAP).renderSemaphore },
            .swapchain = swapchain
        };

        graphicsQueue.Present(presentInfo);
        graphicsQueue.CollectGarbage();
    }

    device.WaitIdle();
    graphicsQueue.CollectGarbage();

    device.DestroyImageView(renderingImageView);
    device.DestroyImage(renderingImage);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
