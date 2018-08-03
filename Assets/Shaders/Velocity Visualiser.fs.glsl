#version 330

uniform sampler2D texture;

layout(location = 0) out vec4 output_colour;

in vec2 surface_texcoord;

void main()
{
    vec2 sample = texture2D(texture, surface_texcoord).rg;
    vec2 colour = (sample + 1.0) * 0.5;
    output_colour = vec4(colour.r, colour.g, 0.0, 1.0);
}
