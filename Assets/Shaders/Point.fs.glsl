#version 330

uniform sampler2D texture;

layout(location = 0) out vec4 output_colour;

in vec4 surface_colour;
noperspective in vec2 surface_texcoord;

void main()
{
    output_colour = surface_colour * texture2D(texture, surface_texcoord);
}
