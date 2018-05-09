#version 330

layout(location = 0) out vec4 output_colour;

uniform vec4 line_colour;

void main()
{
    output_colour = line_colour;
}

