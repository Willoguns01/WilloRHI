#pragma once

#include "WilloRHI.hpp"

#include <memory>
#include <optional>

namespace WilloRHI
{
    struct ShaderModuleInfo
    {
        uint32_t const* byteCode = nullptr;
        uint32_t codeSize = 0;
        std::string name = {};
    };

    class ShaderModule
    {
    public:
        void* GetNativeModuleHandle() const;

        // Even though the returned info contains a pointer to the byteCode,
        // WilloRHI doesn't use it at any point besides initial compilation.
        // The end-user is safe to delete the byteCode once they are done with it.
        ShaderModuleInfo GetInfo() const;

    private:
        friend ImplPipelineManager;
        std::shared_ptr<ImplShaderModule> impl = nullptr;
    };

    struct PipelineStageInfo
    {
        ShaderStageFlag stage = ShaderStageFlag::VERTEX_BIT;
        ShaderModule module;
        std::string entryPoint;
    };

    struct ComputePipelineInfo
    {
        ShaderModule module;
        std::string entryPoint;
        uint32_t pushConstantSize = 0;
        std::string name = {};
    };

    class ComputePipeline
    {
    public:
        void* GetPipelineHandle() const;
        void* GetPipelineLayout() const;
        ComputePipelineInfo GetInfo() const;

    private:
        friend ImplPipelineManager;
        std::shared_ptr<ImplComputePipeline> impl = nullptr;
    };

    struct GraphicsPipelineInfo
    {
        std::string name = {};
        std::optional<PipelineStageInfo> vertexStage = {};
        std::optional<PipelineStageInfo> tessellationControlStage = {};
        std::optional<PipelineStageInfo> tessellationEvaluationStage = {};
        std::optional<PipelineStageInfo> geometryStage = {};
        std::optional<PipelineStageInfo> fragmentStage = {};

        // render state

        uint32_t pushConstantSize = 0;
    };

    class GraphicsPipeline
    {
    public:
        void* GetPipelineHandle() const;
        void* GetPipelineLayout() const;
        GraphicsPipelineInfo GetInfo() const;

    private:
        friend ImplPipelineManager;
        std::shared_ptr<ImplGraphicsPipeline> impl = nullptr;
    };

    class PipelineManager
    {
    public:
        static PipelineManager Create(Device device);

        ShaderModule CompileModule(ShaderModuleInfo moduleInfo);
        ComputePipeline CompileComputePipeline(ComputePipelineInfo pipelineInfo);
        GraphicsPipeline CompileGraphicsPipeline(GraphicsPipelineInfo pipelineInfo);

        ShaderModule GetModule(const std::string& name);
        ComputePipeline GetComputePipeline(const std::string& name);
        GraphicsPipeline GetGraphicsPipeline(const std::string& name);

    private:
        std::shared_ptr<ImplPipelineManager> impl = nullptr;
    };
}
