#include "PerImage.h"
#include "PerObject.h"
#include "PerView.h"

cbuffer PerLine
{
    float line_width;
    float projection_factor;
};

struct VertexInput
{
    float3 position : POSITION;
    float3 direction : TEXCOORD;
    float4 colour : COLOR;
    float2 texcoord : TEXCOORD;
    float side : TEXCOORD;
};

struct VertexOutput
{
    float4 position : SV_POSITION;
    float4 colour : COLOR;
    noperspective float2 texcoord : TEXCOORD;
};

float4 clip_to_image_plane(float4 f, float4 b)
{
    float s = -b.w / (f.w - b.w);
    
    float4 result;
    result.xyz = (s * (f.xyz - b.xyz)) + b.xyz;
    result.w = s;
    return result;
}

VertexOutput vertex_main(in VertexInput input)
{
    VertexOutput output;

    float aspect = viewport_dimensions.x / viewport_dimensions.y;
    
    float4x4 model_view_projection = mul(model, view_projection);
    
    float4 start = mul(float4(input.position, 1.0), model_view_projection);
    float4 end = mul(float4(input.position + input.direction, 1.0), model_view_projection);

    // If either endpoint of the line segment is behind the camera.
    if(end.w < 0.0)
    {
        end = clip_to_image_plane(start, end);
    }
    if(start.w < 0.0)
    {
        start = clip_to_image_plane(end, start);
    }
    
    float2 a = start.xy / start.w;
    a.x *= aspect;
    float2 b = end.xy / end.w;
    b.x *= aspect;

    float2 screen_direction = normalize(a - b);
    float2 lateral = float2(-screen_direction.y, screen_direction.x);
    lateral.x /= aspect;

    float pixel_width_ratio = 2.0 / (viewport_dimensions.x * projection_factor);
    float pixel_width = start.w * pixel_width_ratio;
    float cotangent_fov_over_2 = projection_factor * aspect;
    lateral *= 0.5 * pixel_width * line_width * cotangent_fov_over_2;
    
    start.xy += lateral * input.side;

    output.position = start;
    
    float texture_aspect = texture_dimensions.x / texture_dimensions.y;
    
    float screen_distance = distance(viewport_dimensions.y * a, viewport_dimensions.y * b);
    float texel_distance = screen_distance * texture_aspect / line_width;
    float texcoord_scale = 0.5 * texel_distance;
    
    output.texcoord = float2(input.texcoord.x, input.texcoord.y * texcoord_scale);
    output.colour = input.colour;

    return output;
}

