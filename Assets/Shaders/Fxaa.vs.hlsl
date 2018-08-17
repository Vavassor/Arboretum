#include "PerView.h"

struct VertexInput
{
    float3 position : POSITION;
    float2 texcoord : TEXCOORD;
};

struct VertexOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};

VertexOutput vertex_main(in VertexInput input)
{
    VertexOutput output;
    output.position = mul(float4(input.position, 1.0), view_projection);
    output.texcoord = input.texcoord;
    return output;
}

