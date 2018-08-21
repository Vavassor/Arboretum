// GLSL Vertex Shader "vertex_main"
// Generated by XShaderCompiler
// 21/08/2018 16:48:12

#version 330

in vec3 position;
in vec4 colour;

out vec4 xsv_COLOR0;

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
    gl_Position = ((view_projection * model) * vec4(position, 1.0));
    xsv_COLOR0 = colour;
}

