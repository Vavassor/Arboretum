Texture2D texture0;
SamplerState sampler0;

const float cutoff = 0.3;

struct PixelInput
{
    float4 colour : COLOR;
    noperspective float2 texcoord : TEXCOORD;
};

struct PixelOutput
{
    float4 colour : SV_TARGET;
};

PixelOutput pixel_main(in PixelInput input)
{
    PixelOutput output;
    float4 texture_colour = texture0.Sample(sampler0, input.texcoord);
    clip(texture_colour.a - cutoff);
    output.colour = input.colour * texture_colour;
    return output;
}

