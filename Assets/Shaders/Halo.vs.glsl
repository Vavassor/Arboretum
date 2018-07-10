#version 330

layout(location = 0) in vec3 position;

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

void main()
{
    gl_Position = view_projection * model * vec4(position, 1.0);
}
