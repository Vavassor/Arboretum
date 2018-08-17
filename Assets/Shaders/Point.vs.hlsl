#include "PerObject.h"
#include "PerView.h"

cbuffer PerPoint
{
    float point_radius;
    float projection_factor;
};

struct VertexInput
{
    float3 position : POSITION;
    float2 direction : TEXCOORD;
    float4 colour : COLOR;
    float2 texcoord : TEXCOORD;
};

struct VertexOutput
{
    float4 position : SV_POSITION;
    float4 colour : COLOR;
    noperspective float2 texcoord : TEXCOORD;
};

VertexOutput vertex_main(in VertexInput input)
{
    VertexOutput output;

    float aspect = viewport_dimensions.x / viewport_dimensions.y;
    
    float4 center = mul(float4(input.position, 1.0), mul(model, view_projection));
    
    float2 offset = input.direction;
    offset.x /= aspect;
    float pixel_width_ratio = 2.0 / (viewport_dimensions.x * projection_factor);
    float pixel_width = center.w * pixel_width_ratio;
    float cotangent_fov_over_2 = projection_factor * aspect;
    offset *= pixel_width * point_radius * cotangent_fov_over_2;
    
    float4 corner = center;
    corner.xy += offset;

    output.position = corner;
    output.texcoord = input.texcoord;
    output.colour = input.colour;
    return output;
}

