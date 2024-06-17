#include "ImplPipeline.hpp"

namespace WilloRHI
{
    // module
    ImplShaderModule::~ImplShaderModule() {
        vkDestroyShaderModule(static_cast<VkDevice>(device.GetDeviceNativeHandle()), shaderModule, nullptr);
    }

    void* ShaderModule::GetNativeModuleHandle() const {
        return impl->GetNativeModuleHandle(); }
    void* ImplShaderModule::GetNativeModuleHandle() const {
        return static_cast<void*>(shaderModule);
    }

    ShaderModuleInfo ShaderModule::GetInfo() const {
        return impl->GetInfo(); }
    ShaderModuleInfo ImplShaderModule::GetInfo() const {
        return createInfo;
    }

    // compute
    ImplComputePipeline::~ImplComputePipeline() {
        vkDestroyPipeline(static_cast<VkDevice>(device.GetDeviceNativeHandle()), computePipeline, nullptr);
    }

    void* ComputePipeline::GetPipelineHandle() const {
        return impl->GetPipelineHandle(); }
    void* ImplComputePipeline::GetPipelineHandle() const {
        return static_cast<void*>(computePipeline);
    }

    void* ComputePipeline::GetPipelineLayout() const {
        return impl->GetPipelineLayout(); }
    void* ImplComputePipeline::GetPipelineLayout() const {
        return static_cast<void*>(pipelineLayout);
    }

    ComputePipelineInfo ComputePipeline::GetInfo() const {
        return impl->GetInfo(); }
    ComputePipelineInfo ImplComputePipeline::GetInfo() const {
        return createInfo;
    }

    // graphics
    ImplGraphicsPipeline::~ImplGraphicsPipeline() {
        vkDestroyPipeline(static_cast<VkDevice>(device.GetDeviceNativeHandle()), graphicsPipeline, nullptr);
    }

    void* GraphicsPipeline::GetPipelineHandle() const {
        return impl->GetPipelineHandle(); }
    void* ImplGraphicsPipeline::GetPipelineHandle() const {
        return graphicsPipeline;
    }

    void* GraphicsPipeline::GetPipelineLayout() const {
        return impl->GetPipelineLayout(); }
    void* ImplGraphicsPipeline::GetPipelineLayout() const {
        return static_cast<void*>(pipelineLayout);
    }

    GraphicsPipelineInfo GraphicsPipeline::GetInfo() const {
        return impl->GetInfo(); }
    GraphicsPipelineInfo ImplGraphicsPipeline::GetInfo() const {
        return createInfo;
    }

    uint64_t GraphicsPipeline::GetStageFlags() const {
        return impl->GetStageFlags(); }
    uint64_t ImplGraphicsPipeline::GetStageFlags() const {
        return stageFlags;
    }

    // manager
    PipelineManager PipelineManager::Create(Device device)
    {
        PipelineManager newManager;
        newManager.impl = std::make_shared<ImplPipelineManager>();
        newManager.impl->Init(device);
        return newManager;
    }

    void ImplPipelineManager::Init(Device device)
    {
        this->device = device;
        vkDevice = static_cast<VkDevice>(device.GetDeviceNativeHandle());
    }

    ImplPipelineManager::~ImplPipelineManager()
    {
        Cleanup();
    }

    void ImplPipelineManager::Cleanup()
    {
        _shaderModules.clear();
        _computePipelines.clear();
        _graphicsPipelines.clear();

        for (auto& layout : _pipelineLayouts) {
            vkDestroyPipelineLayout(vkDevice, layout.second, nullptr);
        }
    }

    ShaderModule PipelineManager::CompileModule(ShaderModuleInfo moduleInfo) {
        return impl->CompileModule(moduleInfo); }
    ShaderModule ImplPipelineManager::CompileModule(ShaderModuleInfo moduleInfo) {

        moduleMutex.lock_shared();
        if (_shaderModules.find(moduleInfo.name) != _shaderModules.end()) {
            moduleMutex.unlock_shared();
            device.LogMessage("ShaderModule " + moduleInfo.name + " already exists in manager", false);
            return _shaderModules.at(moduleInfo.name);
        }
        moduleMutex.unlock_shared();

        moduleMutex.lock();
        // in case we've trolled by another thread, not sure if needed?
        if (_shaderModules.find(moduleInfo.name) != _shaderModules.end()) {
            moduleMutex.unlock();
            return _shaderModules.at(moduleInfo.name);
        }

        ShaderModule newModule = {};
        std::shared_ptr<ImplShaderModule> moduleImpl = std::make_shared<ImplShaderModule>();
        newModule.impl = moduleImpl;
        moduleImpl->createInfo = moduleInfo;
        moduleImpl->device = device;

        VkShaderModuleCreateInfo moduleCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .codeSize = moduleInfo.codeSize,
            .pCode = moduleInfo.byteCode
        };

        device.ErrorCheck(vkCreateShaderModule(vkDevice, &moduleCreateInfo, nullptr, &moduleImpl->shaderModule));

        _shaderModules.insert(std::pair(moduleInfo.name, newModule));
        moduleMutex.unlock();

        return newModule;
    }

    ComputePipeline PipelineManager::CompileComputePipeline(ComputePipelineInfo pipelineInfo) {
        return impl->CompileComputePipeline(pipelineInfo); }
    ComputePipeline ImplPipelineManager::CompileComputePipeline(ComputePipelineInfo pipelineInfo) {

        computeMutex.lock_shared();
        if (_computePipelines.find(pipelineInfo.name) != _computePipelines.end()) {
            computeMutex.unlock_shared();
            device.LogMessage("ComputePipeline " + pipelineInfo.name + " already exists in manager", false);
            return _computePipelines.at(pipelineInfo.name);
        }
        computeMutex.unlock_shared();

        computeMutex.lock();
        if (_computePipelines.find(pipelineInfo.name) != _computePipelines.end()) {
            computeMutex.unlock();
            return _computePipelines.at(pipelineInfo.name);
        }

        VkPipelineLayout pipelineLayout = GetLayout(ShaderStageFlag::COMPUTE, pipelineInfo.pushConstantSize);

        ComputePipeline newPipeline = {};
        std::shared_ptr<ImplComputePipeline> pipelineImpl = std::make_shared<ImplComputePipeline>();
        newPipeline.impl = pipelineImpl;
        pipelineImpl->createInfo = pipelineInfo;
        pipelineImpl->device = device;
        pipelineImpl->pipelineLayout = pipelineLayout;

        VkPipelineShaderStageCreateInfo stageCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = static_cast<VkShaderModule>(pipelineInfo.module.GetNativeModuleHandle()),
            .pName = pipelineInfo.entryPoint.c_str(),
            .pSpecializationInfo = nullptr
        };

        VkComputePipelineCreateInfo pipelineCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = stageCreateInfo,
            .layout = pipelineLayout,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = 0
        };

        device.ErrorCheck(vkCreateComputePipelines(vkDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipelineImpl->computePipeline));
        _computePipelines.insert(std::pair(pipelineInfo.name, newPipeline));
        computeMutex.unlock();
        return newPipeline;
    }

    GraphicsPipeline PipelineManager::CompileGraphicsPipeline(GraphicsPipelineInfo pipelineInfo) {
        return impl->CompileGraphicsPipeline(pipelineInfo); }
    GraphicsPipeline ImplPipelineManager::CompileGraphicsPipeline(GraphicsPipelineInfo pipelineInfo)
    {
        graphicsMutex.lock_shared();
        if (_graphicsPipelines.find(pipelineInfo.name) != _graphicsPipelines.end()) {
            graphicsMutex.unlock_shared();
            return _graphicsPipelines.at(pipelineInfo.name);
        }
        graphicsMutex.unlock_shared();

        graphicsMutex.lock();
        if (_graphicsPipelines.find(pipelineInfo.name) != _graphicsPipelines.end()) {
            graphicsMutex.unlock();
            return _graphicsPipelines.at(pipelineInfo.name);
        }

        GraphicsPipeline newPipeline;
        std::shared_ptr<ImplGraphicsPipeline> pipelineImpl = std::make_shared<ImplGraphicsPipeline>();
        newPipeline.impl = pipelineImpl;
        pipelineImpl->createInfo = pipelineInfo;
        pipelineImpl->device = device;

        WilloRHI::ShaderStageFlags stageFlags = {};

        // create modules
        std::vector<VkPipelineShaderStageCreateInfo> vkStages;
        vkStages.reserve(pipelineInfo.stages.size());
        for (const auto& stage : pipelineInfo.stages) {
            vkStages.push_back(VkPipelineShaderStageCreateInfo {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .stage = static_cast<VkShaderStageFlagBits>(stage.stage),
                .module = static_cast<VkShaderModule>(stage.module.GetNativeModuleHandle()),
                .pName = stage.entryPoint.c_str(),
                .pSpecializationInfo = nullptr
            });
            stageFlags |= stage.stage;
        }

        // don't forget to assign pipeline layout
        VkPipelineLayout pipelineLayout = GetLayout(stageFlags, pipelineInfo.pushConstantSize);
        pipelineImpl->pipelineLayout = pipelineLayout;

        // input assembly and input state

        std::vector<VkVertexInputBindingDescription> vkInputBindings;
        vkInputBindings.reserve(pipelineInfo.inputAssemblyInfo.bindings.size());
        for (const auto& binding : pipelineInfo.inputAssemblyInfo.bindings) {
            vkInputBindings.push_back(VkVertexInputBindingDescription{
                .binding = binding.binding,
                .stride = binding.elementStride,
                .inputRate = static_cast<VkVertexInputRate>(binding.inputRate)
            });
        }

        std::vector<VkVertexInputAttributeDescription> vkInputAttribs;
        vkInputAttribs.reserve(pipelineInfo.inputAssemblyInfo.attribs.size());
        for (const auto& attrib : pipelineInfo.inputAssemblyInfo.attribs) {
            vkInputAttribs.push_back(VkVertexInputAttributeDescription{
                .location = attrib.location,
                .binding = attrib.binding,
                .format = static_cast<VkFormat>(attrib.format),
                .offset = attrib.elementOffset
            });
        }

        VkPipelineVertexInputStateCreateInfo vkInputState = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .vertexBindingDescriptionCount = (uint32_t)pipelineInfo.inputAssemblyInfo.bindings.size(),
            .pVertexBindingDescriptions = vkInputBindings.data(),
            .vertexAttributeDescriptionCount = (uint32_t)pipelineInfo.inputAssemblyInfo.attribs.size(),
            .pVertexAttributeDescriptions = vkInputAttribs.data()
        };

        VkPipelineInputAssemblyStateCreateInfo vkInputAssembly = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .topology = static_cast<VkPrimitiveTopology>(pipelineInfo.inputAssemblyInfo.topology),
            .primitiveRestartEnable = pipelineInfo.inputAssemblyInfo.primitiveRestart
        };

        // tessellation

        VkPipelineTessellationDomainOriginStateCreateInfo vkTessellationStateDomain = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_DOMAIN_ORIGIN_STATE_CREATE_INFO,
            .pNext = nullptr,
            .domainOrigin = static_cast<VkTessellationDomainOrigin>(pipelineInfo.tessellationInfo.domainOrigin)
        };

        VkPipelineTessellationStateCreateInfo vkTessellationState = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
            .pNext = &vkTessellationStateDomain,
            .flags = 0,
            .patchControlPoints = pipelineInfo.tessellationInfo.patchControlPoints
        };

        // viewport

        constexpr VkViewport defaultViewport = {.x = 0, .y = 0, .width = 1, .height = 1, .minDepth = 0, .maxDepth = 1};
        constexpr VkRect2D defaultScissor = {.offset = {0, 0}, .extent = {1, 1}};

        VkPipelineViewportStateCreateInfo vkViewportState = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .viewportCount = 1,
            .pViewports = &defaultViewport,
            .scissorCount = 1,
            .pScissors = &defaultScissor
        };

        // rasterization

        VkPipelineRasterizationConservativeStateCreateInfoEXT vkConservativeState = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT,
            .pNext = nullptr,
            .flags = 0,
            .conservativeRasterizationMode = static_cast<VkConservativeRasterizationModeEXT>(pipelineInfo.rasterizerInfo.conservativeRasterInfo.mode),
            .extraPrimitiveOverestimationSize = pipelineInfo.rasterizerInfo.conservativeRasterInfo.size
        };

        VkPipelineRasterizationStateCreateInfo vkRasterizationState = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .pNext = nullptr,//&vkConservativeState,
            .flags = 0,
            .depthClampEnable = pipelineInfo.rasterizerInfo.depthClampEnable,
            .rasterizerDiscardEnable = pipelineInfo.rasterizerInfo.discardEnable,
            .polygonMode = static_cast<VkPolygonMode>(pipelineInfo.rasterizerInfo.polygonMode),
            .cullMode = static_cast<VkCullModeFlags>(pipelineInfo.rasterizerInfo.cullMode),
            .frontFace = static_cast<VkFrontFace>(pipelineInfo.rasterizerInfo.frontFace),
            .depthBiasEnable = pipelineInfo.rasterizerInfo.depthBiasEnable,
            .depthBiasConstantFactor = pipelineInfo.rasterizerInfo.depthBiasConstant,
            .depthBiasClamp = pipelineInfo.rasterizerInfo.depthBiasClamp,
            .depthBiasSlopeFactor = pipelineInfo.rasterizerInfo.depthBiasSlopeFactor,
            .lineWidth = pipelineInfo.rasterizerInfo.lineWidth
        };

        // multisampling

        VkPipelineMultisampleStateCreateInfo vkMultisampleState = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable = VK_FALSE,
            .minSampleShading = 1.0f,
            .pSampleMask = {},
            .alphaToCoverageEnable = {},
            .alphaToOneEnable = {}
        };

        // depth

        VkPipelineDepthStencilStateCreateInfo vkDepthState = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .depthTestEnable = pipelineInfo.depthTestingInfo.depthTestEnable,
            .depthWriteEnable = pipelineInfo.depthTestingInfo.depthWriteEnable,
            .depthCompareOp = static_cast<VkCompareOp>(pipelineInfo.depthTestingInfo.depthTestOp),
            .depthBoundsTestEnable = true,
            .stencilTestEnable = false,
            .front = {},
            .back = {},
            .minDepthBounds = pipelineInfo.depthTestingInfo.minDepthBound,
            .maxDepthBounds = pipelineInfo.depthTestingInfo.maxDepthBound
        };

        // blending

        std::vector<VkFormat> vkAttachmentFormats;
        vkAttachmentFormats.reserve(pipelineInfo.attachments.size());

        std::vector<VkPipelineColorBlendAttachmentState> vkAttachments;
        vkAttachments.reserve(pipelineInfo.attachments.size());
        PipelineAttachmentBlendingInfo noBlend = {};
        for (const auto& attachment : pipelineInfo.attachments) {
            vkAttachmentFormats.push_back(static_cast<VkFormat>(attachment.format));

            vkAttachments.push_back(VkPipelineColorBlendAttachmentState {
                .blendEnable = attachment.blendInfo.has_value(),
                .srcColorBlendFactor = static_cast<VkBlendFactor>(attachment.blendInfo.value_or(noBlend).srcColourBlendFactor),
                .dstColorBlendFactor = static_cast<VkBlendFactor>(attachment.blendInfo.value_or(noBlend).dstColourBlendFactor),
                .colorBlendOp = static_cast<VkBlendOp>(attachment.blendInfo.value_or(noBlend).colourBlendOp),
                .srcAlphaBlendFactor = static_cast<VkBlendFactor>(attachment.blendInfo.value_or(noBlend).srcAlphaBlendFactor),
                .dstAlphaBlendFactor = static_cast<VkBlendFactor>(attachment.blendInfo.value_or(noBlend).dstAlphaBlendFactor),
                .alphaBlendOp = static_cast<VkBlendOp>(attachment.blendInfo.value_or(noBlend).alphaBlendOp),
                .colorWriteMask = static_cast<VkColorComponentFlags>(attachment.blendInfo.value_or(noBlend).colourWriteMask)
            });
        }

        VkPipelineColorBlendStateCreateInfo vkBlendingState = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .logicOpEnable = false,
            .logicOp = VK_LOGIC_OP_NO_OP,
            .attachmentCount = (uint32_t)pipelineInfo.attachments.size(),
            .pAttachments = vkAttachments.data(),
        };

        // dynamic state

        VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

        VkPipelineDynamicStateCreateInfo vkDynamicState = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .dynamicStateCount = 2,
            .pDynamicStates = dynamicStates
        };

        // rendering create info

        VkPipelineRenderingCreateInfo vkRenderingInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .pNext = nullptr,
            .viewMask = {},
            .colorAttachmentCount = (uint32_t)pipelineInfo.attachments.size(),
            .pColorAttachmentFormats = vkAttachmentFormats.data(),
            .depthAttachmentFormat = static_cast<VkFormat>(pipelineInfo.depthTestingInfo.depthAttachmentFormat),
            .stencilAttachmentFormat = VK_FORMAT_UNDEFINED
        };

        // pipeline

        VkGraphicsPipelineCreateInfo pipelineCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext = &vkRenderingInfo,
            .flags = 0,
            .stageCount = (uint32_t)vkStages.size(),
            .pStages = vkStages.data(),
            .pVertexInputState = &vkInputState,
            .pInputAssemblyState = &vkInputAssembly,
            .pTessellationState = &vkTessellationState,
            .pViewportState = &vkViewportState,
            .pRasterizationState = &vkRasterizationState,
            .pMultisampleState = &vkMultisampleState,
            .pDepthStencilState = &vkDepthState,
            .pColorBlendState = &vkBlendingState,
            .pDynamicState = &vkDynamicState,
            .layout = pipelineLayout,
            .renderPass = VK_NULL_HANDLE,
            .subpass = 0,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = 0
        };

        device.ErrorCheck(vkCreateGraphicsPipelines(vkDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipelineImpl->graphicsPipeline));
        _graphicsPipelines.insert(std::pair(pipelineInfo.name, newPipeline));
        graphicsMutex.unlock();
        return newPipeline;
    }

    VkPipelineLayout ImplPipelineManager::GetLayout(ShaderStageFlags stageFlags, uint32_t size)
    {
        uint64_t shifted = (uint64_t)size << 32 | (uint32_t)stageFlags;
        layoutMutex.lock_shared();
        if (_pipelineLayouts.find(shifted) != _pipelineLayouts.end()) {
            layoutMutex.unlock_shared();
            return _pipelineLayouts.at(shifted);
        }
        layoutMutex.unlock_shared();

        layoutMutex.lock();
        // some other thread may or may not have just created this layout while we were waiting on the mutex
        if (_pipelineLayouts.find(shifted) != _pipelineLayouts.end()) {
            layoutMutex.unlock();
            return _pipelineLayouts.at(shifted);
        }

        VkPushConstantRange constantRange = {
            .stageFlags = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_ALL),
            .offset = 0,
            .size = size
        };

        VkDescriptorSetLayout deviceSetLayout = static_cast<VkDescriptorSetLayout>(device.GetNativeSetLayout());

        // create the pipeline layout
        VkPipelineLayoutCreateInfo layoutCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .setLayoutCount = 1,
            .pSetLayouts = &deviceSetLayout,
            .pushConstantRangeCount = size == 0 ? (uint32_t)0 : (uint32_t)1,
            .pPushConstantRanges = size == 0 ? nullptr : &constantRange
        };

        VkPipelineLayout newLayout = VK_NULL_HANDLE;

        vkCreatePipelineLayout(vkDevice, &layoutCreateInfo, nullptr, &newLayout);

        _pipelineLayouts.insert(std::pair(shifted, newLayout));

        layoutMutex.unlock();
        return _pipelineLayouts.at(shifted);
    }
}
