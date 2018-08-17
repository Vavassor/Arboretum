Texture2D texture0;
SamplerState sampler0;

cbuffer PerSpan
{
    float3 tint;
};

struct PixelInput
{
    float2 texcoord : TEXCOORD;
};

struct PixelOutput
{
    float4 colour : SV_TARGET;
};

PixelOutput pixel_main(in PixelInput input)
{
    PixelOutput output;
    float4 colour = texture0.Sample(sampler0, input.texcoord);
    colour.rgb *= tint;
    output.colour = colour;
    return output;
}

