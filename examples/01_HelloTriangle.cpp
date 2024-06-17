#include "ExampleApplication.hpp"

#include <iostream>
#include <fstream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class HelloTriangleApplication : public ExampleApplication
{
private:
    WilloRHI::ImageId renderingImage;
    WilloRHI::ImageViewId renderingImageView;

    WilloRHI::BufferId triangleVertexBuffer;
    WilloRHI::GraphicsPipeline trianglePipeline;

    glm::vec3 positions[3] = {
        {0.f, -.5f, 0.f},
        {-.5f, .5f, 0.f},
        {.5f, .5f, 0.f}
    };

public:
    void Init(bool vsync, uint32_t frameOverlap, bool debug, uint32_t width, uint32_t height, const std::string& applicationName) override
    {
        ExampleApplication::Init(vsync, frameOverlap, debug, width, height, applicationName);

        WilloRHI::ImageCreateInfo renderImageInfo = {
            .dimensions = 2,
            .size = {width, height, 1},
            .numLevels = 1,
            .numLayers = 1,
            .format = WilloRHI::Format::R8G8B8A8_UNORM,
            .usageFlags = WilloRHI::ImageUsageFlag::COLOUR_ATTACHMENT | WilloRHI::ImageUsageFlag::TRANSFER_SRC,
            .createFlags = {},
            .allocationFlags = {},
            .tiling = WilloRHI::ImageTiling::OPTIMAL
        };
        renderingImage = device.CreateImage(renderImageInfo);

        WilloRHI::ImageViewCreateInfo renderViewInfo = {
            .image = renderingImage,
            .viewType = WilloRHI::ImageViewType::VIEW_TYPE_2D,
            .format = WilloRHI::Format::R8G8B8A8_UNORM,
            .subresource = {}
        };
        renderingImageView = device.CreateImageView(renderViewInfo);
    }

    void Load() override
    {
        // vertex buffer
        WilloRHI::BufferCreateInfo bufferInfo = {
            .size = sizeof(float) * 3 * 3,
            .allocationFlags = WilloRHI::AllocationUsageFlag::HOST_ACCESS_SEQUENTIAL_WRITE
        };

        triangleVertexBuffer = device.CreateBuffer(bufferInfo);
        void* bufferPtr = device.GetBufferPointer(triangleVertexBuffer);
        memcpy(bufferPtr, positions, sizeof(float) * 3 * 3);

        // vertex shader
        std::ifstream inFile("resources/shaders/coloured_triangle_vert.slang.spv", std::ios::binary | std::ios::ate);
        size_t vertShaderSize = inFile.tellg();
        uint32_t* vertexShaderData = new uint32_t[vertShaderSize / sizeof(uint32_t)];
        inFile.seekg(std::ios::beg);
        inFile.read((char*)vertexShaderData, vertShaderSize);
        inFile.close();

        // fragment shader
        inFile.open("resources/shaders/coloured_triangle_frag.slang.spv", std::ios::binary | std::ios::ate);
        size_t fragShaderSize = inFile.tellg();
        uint32_t* fragmentShaderData = new uint32_t[fragShaderSize / sizeof(uint32_t)];
        inFile.seekg(std::ios::beg);
        inFile.read((char*)fragmentShaderData, fragShaderSize);
        inFile.close();

        // graphics pipeline
        WilloRHI::GraphicsPipelineInfo pipelineInfo = {
            .name = "Triangle Pipeline",
            .stages = {
                WilloRHI::PipelineStageInfo {
                    .stage = WilloRHI::ShaderStageFlag::VERTEX,
                    .module = pipelineManager.CompileModule(WilloRHI::ShaderModuleInfo {
                        .byteCode = vertexShaderData,
                        .codeSize = (uint32_t)vertShaderSize,
                        .name = "Triangle Vertex"
                    }),
                    .entryPoint = "main"
                },
                WilloRHI::PipelineStageInfo {
                    .stage = WilloRHI::ShaderStageFlag::FRAGMENT,
                    .module = pipelineManager.CompileModule(WilloRHI::ShaderModuleInfo {
                        .byteCode = fragmentShaderData,
                        .codeSize = (uint32_t)fragShaderSize,
                        .name = "Triangle Fragment"
                    }),
                    .entryPoint = "main"
                }
            },
            .inputAssemblyInfo = {
                .topology = WilloRHI::PrimitiveTopology::TRIANGLE_LIST,
                .primitiveRestart = false,
                .bindings = { {
                        .binding = 0,
                        .elementStride = sizeof(float) * 3,
                        .inputRate = WilloRHI::VertexInputRate::VERTEX
                    }
                },
                .attribs = { {
                    .location = 0,
                    .binding = 0,
                    .format = WilloRHI::Format::R32G32B32_SFLOAT,
                    .elementOffset = 0
                    }
                }
            },
            .attachments = {
                WilloRHI::PipelineAttachmentInfo {
                    .format = WilloRHI::Format::R8G8B8A8_UNORM
                }
            },
            .pushConstantSize = 0
        };

        trianglePipeline = pipelineManager.CompileGraphicsPipeline(pipelineInfo);
    }

    void Update() override
    {
        ExampleApplication::Update();
    }

    void Render(WilloRHI::ImageId swapchainImage, WilloRHI::BinarySemaphore acquireSemaphore, WilloRHI::BinarySemaphore renderSemaphore) override
    {
        WilloRHI::CommandList cmdList = graphicsQueue.GetCmdList();
        cmdList.Begin();

        WilloRHI::ImageMemoryBarrierInfo barrierInfo = {
            .dstStage = WilloRHI::PipelineStageFlag::COLOUR_ATTACHMENT_OUTPUT,
            .dstAccess = WilloRHI::MemoryAccessFlag::WRITE,
            .dstLayout = WilloRHI::ImageLayout::ATTACHMENT
        };
        cmdList.ImageMemoryBarrier(renderingImage, barrierInfo);

        cmdList.BeginRendering({
            .colourAttachments = {{
                .imageView = renderingImageView,
                .imageLayout = WilloRHI::ImageLayout::ATTACHMENT,
                .loadOp = WilloRHI::LoadOp::CLEAR,
                .storeOp = WilloRHI::StoreOp::STORE,
                .clearColour = {0.1f, 0.1f, 0.1f, 1.0f}
            }},
            .renderingArea = { .extent = {renderingWidth, renderingHeight} }
        });

        cmdList.BindGraphicsPipeline(trianglePipeline);
        cmdList.BindVertexBuffer(triangleVertexBuffer, 0);
        cmdList.Draw(3, 1, 0, 0);

        cmdList.EndRendering();

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
            .signalTimelineSemaphores = { { timelineSemaphore, frameNum } },
            .waitBinarySemaphores = { acquireSemaphore },
            .signalBinarySemaphores = { renderSemaphore },
            .commandLists = { cmdList }
        };

        graphicsQueue.Submit(submitInfo);
    }

    void OnResize(uint32_t width, uint32_t height) override
    {
        ExampleApplication::OnResize(width, height);

        device.DestroyImage(renderingImage);
        device.DestroyImageView(renderingImageView);

        WilloRHI::ImageCreateInfo renderImageInfo = {
            .dimensions = 2,
            .size = {width, height, 1},
            .numLevels = 1,
            .numLayers = 1,
            .format = WilloRHI::Format::R8G8B8A8_UNORM,
            .usageFlags = WilloRHI::ImageUsageFlag::COLOUR_ATTACHMENT | WilloRHI::ImageUsageFlag::TRANSFER_SRC,
            .createFlags = {},
            .allocationFlags = {},
            .tiling = WilloRHI::ImageTiling::OPTIMAL
        };
        renderingImage = device.CreateImage(renderImageInfo);

        WilloRHI::ImageViewCreateInfo renderViewInfo = {
            .image = renderingImage,
            .viewType = WilloRHI::ImageViewType::VIEW_TYPE_2D,
            .format = WilloRHI::Format::R8G8B8A8_UNORM,
            .subresource = {}
        };
        renderingImageView = device.CreateImageView(renderViewInfo);
    }

    void Cleanup()
    {
        device.DestroyBuffer(triangleVertexBuffer);

        device.DestroyImage(renderingImage);
        device.DestroyImageView(renderingImageView);
    }
};

int main()
{
    HelloTriangleApplication app;
    app.Init(true, 3, true, 1280, 720, "01_HelloTriangle");
    app.Load();
    app.Run();
    app.Cleanup();
}
