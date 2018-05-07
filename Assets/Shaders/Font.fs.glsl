#version 330

uniform sampler2D texture;
uniform vec3 text_colour;

layout(location = 0) out vec4 output_colour;

in vec2 surface_texcoord;

void main()
{
    vec4 colour = texture2D(texture, surface_texcoord);
    colour.rgb *= text_colour;
    output_colour = colour;
}

