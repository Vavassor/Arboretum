#version 330

layout(location = 0) in vec3 position;
layout(location = 1) in vec4 colour;

layout(std140) uniform PerObject
{
    mat4x4 model;
    mat4x4 normal_matrix;
};

layout(std140) uniform PerView
{
    mat4x4 view_projection;
    vec2 viewport_dimensions;
};

out vec4 surface_colour;

void main()
{
    gl_Position = view_projection * model * vec4(position, 1.0);
    surface_colour = colour;
}
