struct PixelOutput
{
    float4 colour : SV_TARGET;
};

struct PixelInput
{
    float4 colour : COLOR;
};

PixelOutput pixel_main(in PixelInput input)
{
    PixelOutput output;
    output.colour = input.colour;
    return output;
}

