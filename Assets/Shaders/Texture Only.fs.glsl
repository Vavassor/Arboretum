#version 330

uniform sampler2D texture;

layout(location = 0) out vec4 output_colour;

in vec2 surface_texcoord;

void main()
{
    output_colour = vec4(texture2D(texture, surface_texcoord).rgb, 1.0);
}

