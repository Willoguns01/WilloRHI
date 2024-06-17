
[shader("vertex")]
float4 main(float3 input : POSITION) : SV_Position
{
    return float4(input, 1.0f);
}
