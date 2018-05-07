#version 330

layout(location = 0) in vec3 position;
layout(location = 2) in vec4 colour;

uniform mat4x4 model_view_projection;

out vec3 surface_texcoord;
out vec4 surface_colour;

void main()
{
    vec4 surface_position = model_view_projection * vec4(position, 1.0);
    gl_Position = surface_position;
    surface_texcoord = vec3(surface_position.xy, surface_position.w);
    surface_colour = colour;
}

