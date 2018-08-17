Texture2D texture0;
SamplerState sampler0;

struct PixelInput
{
    float3 texcoord : TEXCOORD;
    float4 colour : COLOR;
};

struct PixelOutput
{
    float4 colour : SV_TARGET;
};

PixelOutput pixel_main(in PixelInput input)
{
    PixelOutput output;
    float2 screen_texcoord = input.texcoord.xy / input.texcoord.z;
    output.colour = input.colour * texture0.Sample(sampler0, screen_texcoord);
    return output;
}

