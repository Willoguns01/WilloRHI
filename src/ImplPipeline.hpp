#include "WilloRHI/Pipeline.hpp"

#include <vulkan/vulkan.h>

#include <unordered_map>
#include <mutex>
#include <shared_mutex>

namespace WilloRHI
{
    struct ImplShaderModule
    {
        VkShaderModule shaderModule = VK_NULL_HANDLE;
        ShaderModuleInfo createInfo = {};

        Device device;

        ~ImplShaderModule();

        void* GetNativeModuleHandle() const;
        ShaderModuleInfo GetInfo() const;
    };

    struct ImplComputePipeline
    {
        VkPipeline computePipeline = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        ComputePipelineInfo createInfo = {};

        Device device;

        ~ImplComputePipeline();

        void* GetPipelineHandle() const;
        void* GetPipelineLayout() const;
        ComputePipelineInfo GetInfo() const;
    };

    struct ImplGraphicsPipeline
    {
        VkPipeline graphicsPipeline = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        GraphicsPipelineInfo createInfo = {};
        VkPipelineStageFlags stageFlags = VK_PIPELINE_STAGE_FLAG_BITS_MAX_ENUM;

        Device device;

        ~ImplGraphicsPipeline();

        void* GetPipelineHandle() const;
        void* GetPipelineLayout() const;
        GraphicsPipelineInfo GetInfo() const;
        uint64_t GetStageFlags() const;
    };

    struct ImplPipelineManager
    {
        Device device;
        VkDevice vkDevice = VK_NULL_HANDLE;

        std::unordered_map<std::string, ShaderModule> _shaderModules;
        std::shared_mutex moduleMutex;

        std::unordered_map<std::string, ComputePipeline> _computePipelines;
        std::shared_mutex computeMutex;

        std::unordered_map<std::string, GraphicsPipeline> _graphicsPipelines;
        std::shared_mutex graphicsMutex;

        // access with push constant size, because that's the only thing changing
        std::unordered_map<uint64_t, VkPipelineLayout> _pipelineLayouts;
        std::shared_mutex layoutMutex;

        void Init(Device device);

        ~ImplPipelineManager();
        void Cleanup();

        ShaderModule CompileModule(ShaderModuleInfo moduleInfo);
        ComputePipeline CompileComputePipeline(ComputePipelineInfo pipelineInfo);
        GraphicsPipeline CompileGraphicsPipeline(GraphicsPipelineInfo pipelineInfo);

        ShaderModule GetModule(const std::string& name);
        ComputePipeline GetComputePipeline(const std::string& name);
        GraphicsPipeline GetGraphicsPipeline(const std::string& name);

        VkPipelineLayout GetLayout(ShaderStageFlags stageFlags, uint32_t size);
    };
}
