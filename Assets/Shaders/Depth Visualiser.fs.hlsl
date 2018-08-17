Texture2D texture0;
SamplerState sampler0;

cbuffer DepthBlock
{
    float near;
    float far;
};

struct PixelInput
{
    float2 texcoord : TEXCOORD;
};

struct PixelOutput
{
    float4 colour : SV_TARGET;
};

float linearise_depth(float x, float n, float f)
{
    return (2.0 * n) / (f + n - x * (f - n));
}

PixelOutput pixel_main(in PixelInput input)
{
    PixelOutput output;
    float depth = texture0.Sample(sampler0, input.texcoord).r;
    float value = linearise_depth(depth, near, far);
    output.colour = float4(float3(value), 1.0);
    return output;
}

