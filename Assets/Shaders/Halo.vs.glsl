#version 330

layout(location = 0) in vec3 position;

uniform mat4x4 model_view_projection;

void main()
{
    gl_Position = model_view_projection * vec4(position, 1.0);
}

