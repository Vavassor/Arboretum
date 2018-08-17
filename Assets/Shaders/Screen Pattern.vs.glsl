// GLSL Vertex Shader "vertex_main"
// Generated by XShaderCompiler
// 16/08/2018 23:04:15

#version 330

in vec3 position;
in vec4 colour;

out vec3 xsv_TEXCOORD0;
out vec4 xsv_COLOR0;

layout(std140) uniform PerImage
{
    vec2 texture_dimensions;
};

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

void main()
{
    vec4 surface_position = ((view_projection * model) * vec4(position, 1.0));
    gl_Position = surface_position;
    vec2 uv = surface_position.xy;
    uv *= viewport_dimensions;
    uv /= texture_dimensions;
    xsv_TEXCOORD0 = vec3(uv, surface_position.w);
    xsv_COLOR0 = colour;
}

