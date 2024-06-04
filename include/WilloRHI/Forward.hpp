#pragma once

namespace WilloRHI
{
    struct BinarySemaphore;
    struct ImplBinarySemaphore;

    struct TimelineSemaphore;
    struct ImplTimelineSemaphore;

    class Device;
    struct ImplDevice;

    class CommandList;
    struct ImplCommandList;

    class Swapchain;
    struct ImplSwapchain;
    
    class Queue;
    struct ImplQueue;

    class ShaderModule;
    struct ImplShaderModule;

    class ComputePipeline;
    struct ImplComputePipeline;

    class GraphicsPipeline;
    struct ImplGraphicsPipeline;

    class PipelineManager;
    struct ImplPipelineManager;
}
