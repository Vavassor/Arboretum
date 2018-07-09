#version 330

uniform sampler2D texture;

layout(std140) uniform PerSpan
{
    vec3 tint;
};

layout(location = 0) out vec4 output_colour;

in vec2 surface_texcoord;

void main()
{
    vec4 colour = texture2D(texture, surface_texcoord);
    colour.rgb *= tint;
    output_colour = colour;
}

