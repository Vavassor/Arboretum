#include "PerImage.h"
#include "PerObject.h"
#include "PerView.h"

struct VertexInput
{
    float3 position : POSITION;
    float4 colour : COLOR;
};

struct VertexOutput
{
    float4 position : SV_POSITION;
    float3 texcoord : TEXCOORD;
    float4 colour : COLOR;
};

VertexOutput vertex_main(in VertexInput input)
{
    VertexOutput output;
    float4 surface_position = mul(float4(input.position, 1.0), mul(model, view_projection));
    output.position = surface_position;

    float2 uv = surface_position.xy;
    uv *= viewport_dimensions;
    uv /= texture_dimensions;
    output.texcoord = float3(uv, surface_position.w);

    output.colour = input.colour;
    return output;
}

