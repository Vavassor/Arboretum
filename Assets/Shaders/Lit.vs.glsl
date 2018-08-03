#version 330

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec4 colour;

layout(std140) uniform PerObject
{
    mat4x4 model;
    mat4x4 prior_model;
    mat4x4 normal_matrix;
};

layout(std140) uniform PerView
{
    mat4x4 view_projection;
    mat4x4 prior_view_projection;
    vec2 viewport_dimensions;
};

out vec3 surface_normal;
out vec3 surface_colour;
out vec4 clip_space_position;
out vec4 prior_clip_space_position;

void main()
{
    vec4 position4 = vec4(position, 1.0);
    clip_space_position = view_projection * model * position4;
    prior_clip_space_position = prior_view_projection * prior_model * position4;
    
    gl_Position = clip_space_position;
    surface_normal = (normal_matrix * vec4(normal, 0.0)).xyz;
    surface_colour = colour.rgb;
}

