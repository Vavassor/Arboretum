#include "PerObject.h"
#include "PerView.h"

struct VertexInput
{
    float3 position : POSITION;
    float2 texcoord : TEXCOORD;
};

struct VertexOutput
{
    float4 position : SV_POSITION;
};

VertexOutput vertex_main(in VertexInput input)
{
    VertexOutput output;
    output.position = mul(float4(input.position, 1.0), mul(model, view_projection));
    return output;
}

