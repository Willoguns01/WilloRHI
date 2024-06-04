#include "shared.slang"

struct TestComputeConstants {
    uint32_t targetTexture;
};

[[vk::push_constant]] TestComputeConstants pushConstants;

[shader("compute")]
[numthreads(16,16,1)]
void computeMain(uint3 globalThreadId : SV_DispatchThreadID, uint3 localThreadId : SV_GroupThreadID)
{
    RWTexture2D<float4> outputTexture = RWTexture2D<float4>::Get(pushConstants.targetTexture);

    int2 texelCoord = globalThreadId.xy;
    int2 outputSize;
    outputTexture.GetDimensions(outputSize.x, outputSize.y);

    if (texelCoord.x < outputSize.x && texelCoord.y < outputSize.y)
    {
        float4 colour = float4(0, 0, 0, 1.0);

        if (localThreadId.x != 0 && localThreadId.y != 0)
        {
            colour.x = float(texelCoord.x) / (outputSize.x);
            colour.y = float(texelCoord.y) / (outputSize.y);
        }

        outputTexture[texelCoord] = colour;
    }
}
