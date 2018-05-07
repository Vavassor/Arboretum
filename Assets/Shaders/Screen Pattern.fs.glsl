#version 330

uniform sampler2D texture;
uniform vec2 pattern_scale;
uniform vec2 viewport_dimensions;
uniform vec2 texture_dimensions;

layout(location = 0) out vec4 output_colour;

in vec3 surface_texcoord;
in vec4 surface_colour;

void main()
{
    vec2 screen_texcoord = surface_texcoord.xy / surface_texcoord.z;
    screen_texcoord *= viewport_dimensions;
    screen_texcoord /= texture_dimensions;
    screen_texcoord *= pattern_scale;
    output_colour = surface_colour * texture2D(texture, screen_texcoord);
}

