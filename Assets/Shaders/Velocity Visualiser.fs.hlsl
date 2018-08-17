Texture2D texture0;
SamplerState sampler0;

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
    float2 value = texture0.Sample(sampler0, input.texcoord).rg;
    float2 colour = (value + 1.0) * 0.5;
    output.colour = float4(colour.r, colour.g, 0.0, 1.0);
    return output;
}

