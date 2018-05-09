#version 330

layout(location = 0) out vec4 output_colour;

in vec4 surface_colour;

void main()
{
    output_colour = surface_colour;
}

