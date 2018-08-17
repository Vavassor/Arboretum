#include "PerObject.h"
#include "PerView.h"

struct VertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 colour : COLOR;
};

struct VertexOutput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float3 colour : COLOR;
};

VertexOutput vertex_main(in VertexInput input)
{
    VertexOutput output;
    output.position = mul(float4(input.position, 1.0), mul(model, view_projection));
    output.normal = mul(float4(input.normal, 0.0), normal_matrix).xyz;
    output.colour = input.colour.rgb;
    return output;
}

