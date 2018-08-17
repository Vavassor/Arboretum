cbuffer HaloBlock
{
    float4 halo_colour;
};

struct PixelOutput
{
    float4 colour : SV_TARGET;
};

PixelOutput pixel_main()
{
    PixelOutput output;
    output.colour = halo_colour;
    return output;
}

