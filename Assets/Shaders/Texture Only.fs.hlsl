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
    output.colour = float4(texture0.Sample(sampler0, input.texcoord).rgb, 1.0);
    return output;
}

