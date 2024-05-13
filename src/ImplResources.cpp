#include "ImplResources.hpp"

bool WilloRHI::IsDepthFormat(Format format)
{
    switch (format)
    {
        case Format::D16_UNORM:
            return true;
        case Format::X8_D24_UNORM_PACK32:
            return true;
        case Format::D32_SFLOAT:
            return true;
        case Format::S8_UINT:
            return true;
        case Format::D16_UNORM_S8_UINT:
            return true;
        case Format::D24_UNORM_S8_UINT:
            return true;
        case Format::D32_SFLOAT_S8_UINT:
            return true;
        default: 
            return false;
    }
}

bool WilloRHI::IsStencilFormat(Format format)
{
    switch (format)
    {
        case Format::S8_UINT:
            return true;
        case Format::D16_UNORM_S8_UINT:
            return true;
        case Format::D24_UNORM_S8_UINT:
            return true;
        case Format::D32_SFLOAT_S8_UINT:
            return true;
        default:
            return false;
    }
}

VkImageAspectFlags WilloRHI::AspectFromFormat(Format format)
{
    if (IsDepthFormat(format) || IsStencilFormat(format))
    {
        return (IsDepthFormat(format) ? VK_IMAGE_ASPECT_DEPTH_BIT : 0) | (IsStencilFormat(format) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
    }
    return VK_IMAGE_ASPECT_COLOR_BIT;
}
