#include "ImplCommandList.hpp"

namespace WilloRHI
{
    void CommandList::Reset() {
        impl->Reset();
    }

    void CommandList::Begin() {
        impl->Begin();
    }

    void CommandList::End() {
        impl->End();
    }

    void CommandList::ClearImage(ImageId image, const float clearColour[4], const ImageSubresourceRange& subresourceRange) {
        impl->ClearImage(image, clearColour, subresourceRange);
    }

    void CommandList::TransitionImageLayout(ImageId image, const ImageMemoryBarrierInfo& barrierInfo) {
        impl->TransitionImageLayout(image, barrierInfo);
    }
}
