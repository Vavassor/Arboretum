#version 330

layout(location = 0) in vec3 position;
layout(location = 2) in vec4 colour;

uniform mat4x4 model_view_projection;

out vec4 surface_colour;

void main()
{
    gl_Position = model_view_projection * vec4(position, 1.0);
    surface_colour = colour;
}

