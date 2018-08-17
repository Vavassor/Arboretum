cbuffer LightBlock
{
    float3 light_direction;
};

struct PixelInput
{
    float3 normal : NORMAL;
    float3 colour : COLOR;
};

struct PixelOutput
{
    float4 colour : SV_TARGET;
};

float half_lambert(float3 n, float3 l)
{
    return 0.5 * dot(n, l) + 0.5;
}

float lambert(float3 n, float3 l)
{
    return max(dot(n, l), 0.0);
}

PixelOutput pixel_main(in PixelInput input)
{
    PixelOutput output;
    float light = half_lambert(input.normal, light_direction);
    output.colour = float4(input.colour * float3(light), 1.0);
    return output;
}

