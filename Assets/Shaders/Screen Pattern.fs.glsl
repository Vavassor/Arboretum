#version 330

uniform sampler2D texture;

layout(location = 0) out vec4 output_colour;

in vec3 surface_texcoord;
in vec4 surface_colour;

void main()
{
    vec2 screen_texcoord = surface_texcoord.xy / surface_texcoord.z;
    output_colour = surface_colour * texture2D(texture, screen_texcoord);
}

