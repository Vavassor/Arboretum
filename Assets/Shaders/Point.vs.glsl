// GLSL Vertex Shader "vertex_main"
// Generated by XShaderCompiler
// 21/08/2018 16:48:12

#version 330

in vec3 position;
in vec2 direction;
in vec4 colour;
in vec2 texcoord;

              out vec4 xsv_COLOR0;
noperspective out vec2 xsv_TEXCOORD0;

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

layout(std140) uniform PerPoint
{
    float point_radius;
    float projection_factor;
};

void main()
{
    float aspect = viewport_dimensions.x / viewport_dimensions.y;
    vec4 center = ((view_projection * model) * vec4(position, 1.0));
    vec2 offset = direction;
    offset.x /= aspect;
    float pixel_width_ratio = 2.0f / (viewport_dimensions.x * projection_factor);
    float pixel_width = center.w * pixel_width_ratio;
    float cotangent_fov_over_2 = projection_factor * aspect;
    offset *= vec2(pixel_width * point_radius * cotangent_fov_over_2);
    vec4 corner = center;
    corner.xy += offset;
    gl_Position = corner;
    xsv_TEXCOORD0 = texcoord;
    xsv_COLOR0 = colour;
}

