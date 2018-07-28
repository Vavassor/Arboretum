#version 330

layout(location = 0) in vec3 position;
layout(location = 1) in vec4 colour;

layout(std140) uniform PerImage
{
    vec2 texture_dimensions;
};

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

out vec3 surface_texcoord;
out vec4 surface_colour;

void main()
{
    vec4 surface_position = view_projection * model * vec4(position, 1.0);
    gl_Position = surface_position;
    
    vec2 texcoord = surface_position.xy;
    texcoord *= viewport_dimensions;
    texcoord /= texture_dimensions;
    surface_texcoord = vec3(texcoord, surface_position.w);
    surface_colour = colour;
}

