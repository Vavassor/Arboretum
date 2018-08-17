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
    float4 colour : COLOR;
};

VertexOutput vertex_main(in VertexInput input)
{
    VertexOutput output;
    output.position = mul(float4(input.position, 1.0), mul(model, view_projection));
    output.colour = input.colour;
    return output;
}

