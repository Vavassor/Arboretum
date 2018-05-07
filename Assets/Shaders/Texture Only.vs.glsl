#version 330

layout(location = 0) in vec3 position;
layout(location = 3) in vec2 texcoord;

uniform mat4x4 model_view_projection;

out vec2 surface_texcoord;

void main()
{
    gl_Position = model_view_projection * vec4(position, 1.0);
    surface_texcoord = texcoord;
}

