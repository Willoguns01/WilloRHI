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
        ShaderStageFlag stage = ShaderStageFlag::VERTEX;
        ShaderModule module;
        std::string entryPoint;
    };

    // compute pipeline

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

    // graphics pipeline

    struct VertexBindingInfo {
        uint32_t binding = 0;
        uint32_t elementStride = 0;
        VertexInputRate inputRate = VertexInputRate::VERTEX;
    };

    struct VertexAttributeInfo {
        uint32_t location = 0;
        uint32_t binding = 0;
        Format format = Format::UNDEFINED;
        uint32_t elementOffset = 0;
    };

    struct PipelineInputAssemblyInfo
    {
        PrimitiveTopology topology = PrimitiveTopology::TRIANGLE_LIST;
        bool primitiveRestart = false;
        std::vector<VertexBindingInfo> bindings;
        std::vector<VertexAttributeInfo> attribs;
    };

    struct PipelineTessellationInfo
    {
        uint32_t patchControlPoints = 0;
        TessellationDomainOrigin domainOrigin = TessellationDomainOrigin::UPPER_LEFT;
    };

    struct PipelineDepthTestingInfo
    {
        Format depthAttachmentFormat = Format::UNDEFINED;
        bool depthTestEnable = false;
        bool depthWriteEnable = false;
        CompareOp depthTestOp = CompareOp::LESS_OR_EQUAL;
        float minDepthBound = 0.0f;
        float maxDepthBound = 1.0f;
    };

    struct PipelineConservativeRasterInfo
    {
        ConservativeRasterMode mode = ConservativeRasterMode::NONE;
        float size = 0.0f;
    };

    struct PipelineAttachmentBlendingInfo
    {
        BlendFactor srcColourBlendFactor = BlendFactor::ONE;
        BlendFactor dstColourBlendFactor = BlendFactor::ZERO;
        BlendOp colourBlendOp = BlendOp::ADD;
        BlendFactor srcAlphaBlendFactor = BlendFactor::ONE;
        BlendFactor dstAlphaBlendFactor = BlendFactor::ZERO;
        BlendOp alphaBlendOp = BlendOp::ADD;
        ColourComponentFlags colourWriteMask = ColourComponentFlag::COMP_R | ColourComponentFlag::COMP_G | ColourComponentFlag::COMP_B | ColourComponentFlag::COMP_A;
    };

    struct PipelineAttachmentInfo
    {
        Format format;
        std::optional<PipelineAttachmentBlendingInfo> blendInfo;
    };

    struct PipelineRasterizerInfo
    {
        bool depthClampEnable = false;
        bool discardEnable = false;
        PolygonMode polygonMode = PolygonMode::FILL;
        CullMode cullMode = CullMode::NONE;
        FrontFace frontFace = FrontFace::CLOCKWISE;
        bool depthBiasEnable = false;
        float depthBiasConstant = 0.0f;
        float depthBiasClamp = 0.0f;
        float depthBiasSlopeFactor = 0.0f;
        float lineWidth = 1.0f;
        PipelineConservativeRasterInfo conservativeRasterInfo = {};
    };

    struct GraphicsPipelineInfo
    {
        std::string name = {};
        std::vector<PipelineStageInfo> stages;

        // render state
        PipelineInputAssemblyInfo inputAssemblyInfo = {};
        PipelineTessellationInfo tessellationInfo = {};
        PipelineDepthTestingInfo depthTestingInfo = {};
        PipelineRasterizerInfo rasterizerInfo = {};

        std::vector<PipelineAttachmentInfo> attachments;

        uint32_t pushConstantSize = 0;
    };

    class GraphicsPipeline
    {
    public:
        void* GetPipelineHandle() const;
        void* GetPipelineLayout() const;
        GraphicsPipelineInfo GetInfo() const;
        uint64_t GetStageFlags() const;

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
