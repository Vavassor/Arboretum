// GLSL Vertex Shader "vertex_main"
// Generated by XShaderCompiler
// 23/08/2018 15:37:34

#version 330

in vec3  position;
in vec3  direction;
in float side;

layout(std140, row_major) uniform PerObject
{
    mat4 model;
    mat4 normal_matrix;
};

layout(std140, row_major) uniform PerView
{
    mat4 view_projection;
    vec2 viewport_dimensions;
};

layout(std140) uniform PerLine
{
    float line_width;
    float projection_factor;
};

vec4 clip_to_image_plane(vec4 f, vec4 b)
{
    float s = -b.w / (f.w - b.w);
    vec4 result;
    result.xyz = (s * (f.xyz - b.xyz)) + b.xyz;
    result.w = s;
    return result;
}

void main()
{
    float aspect = viewport_dimensions.x / viewport_dimensions.y;
    mat4 model_view_projection = (view_projection * model);
    vec4 start = (model_view_projection * vec4(position, 1.0));
    vec4 end = (model_view_projection * vec4(position + direction, 1.0));
    if (end.w < 0.0f)
    {
        end = clip_to_image_plane(start, end);
    }
    if (start.w < 0.0f)
    {
        start = clip_to_image_plane(end, start);
    }
    vec2 a = start.xy / start.w;
    a.x *= aspect;
    vec2 b = end.xy / end.w;
    b.x *= aspect;
    vec2 screen_direction = normalize(a - b);
    vec2 lateral = vec2(-screen_direction.y, screen_direction.x);
    lateral.x /= aspect;
    float pixel_width_ratio = 2.0f / (viewport_dimensions.x * projection_factor);
    float pixel_width = start.w * pixel_width_ratio;
    float cotangent_fov_over_2 = projection_factor * aspect;
    lateral *= vec2(0.5f * pixel_width * line_width * cotangent_fov_over_2);
    start.xy += lateral * side;
    gl_Position = start;
}

