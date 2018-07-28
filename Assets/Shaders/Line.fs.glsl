#version 330

uniform sampler2D texture;
const float cutoff = 0.3;

layout(location = 0) out vec4 output_colour;

in vec4 surface_colour;
noperspective in vec2 surface_texcoord;

void main()
{
    vec4 texture_colour = texture2D(texture, surface_texcoord);
    if(texture_colour.a < cutoff)
    {
        discard;
    }
    output_colour = surface_colour * texture_colour;
}

