#version 330

layout(location = 0) out vec4 output_colour;

uniform vec4 halo_colour;

void main()
{
    output_colour = halo_colour;
}

