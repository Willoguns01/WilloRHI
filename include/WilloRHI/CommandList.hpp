#pragma once

#include "WilloRHI.hpp"

namespace WilloRHI
{
    struct ImageSubresourceRange 
    {
        uint32_t baseLevel = 0;
        uint32_t numLevels = 0;
        uint32_t baseLayer = 0;
        uint32_t numLayers = 0;
    };

    struct ImageMemoryBarrierInfo
    {
        PipelineStageBits srcStage = PipelineStage::NONE;
        PipelineStageBits dstStage = PipelineStage::NONE;
        MemoryAccessBits srcAccess = MemoryAccess::NONE;
        MemoryAccessBits dstAccess = MemoryAccess::NONE;
        ImageLayout srcLayout = ImageLayout::UNDEFINED;
        ImageLayout dstLayout = ImageLayout::UNDEFINED;
        ImageSubresourceRange subresourceRange = {};
    };

    class CommandList
    {
        public:
        CommandList() = default;

        void Reset();

        void Begin();
        void End();

        void ClearImage(ImageId image, const float clearColour[4], const ImageSubresourceRange& subresourceRange);

        void TransitionImageLayout(ImageId image, const ImageMemoryBarrierInfo& barrierInfo);

        protected:
        friend ImplDevice;
        std::shared_ptr<ImplCommandList> impl = nullptr;
    };
}
