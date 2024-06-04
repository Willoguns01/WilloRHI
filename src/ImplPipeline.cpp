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

        VkPipelineLayout pipelineLayout = GetLayout(ShaderStageFlag::COMPUTE_BIT, pipelineInfo.pushConstantSize);

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

        // don't forget to assign pipeline layout

        VkPipelineShaderStageCreateInfo pipelineStageInfo = {

        };

        VkGraphicsPipelineCreateInfo pipelineCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0
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
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &constantRange
        };

        VkPipelineLayout newLayout = VK_NULL_HANDLE;

        vkCreatePipelineLayout(vkDevice, &layoutCreateInfo, nullptr, &newLayout);

        _pipelineLayouts.insert(std::pair(shifted, newLayout));

        layoutMutex.unlock();
        return _pipelineLayouts.at(shifted);
    }
}
