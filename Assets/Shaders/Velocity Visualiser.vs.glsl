#version 330

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texcoord;

layout(std140) uniform PerView
{
    mat4x4 view_projection;
    vec2 viewport_dimensions;
};

out vec2 surface_texcoord;

void main()
{
    gl_Position = view_projection * vec4(position, 1.0);
    surface_texcoord = texcoord;
}
