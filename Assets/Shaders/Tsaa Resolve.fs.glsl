#version 330

uniform sampler2D history;
uniform sampler2D texture;

layout(std140) uniform TsaaResolveBlock
{
    vec2 uv_jitter;
};

layout(location = 0) out vec4 output_colour;

in vec2 surface_texcoord;

void main()
{
    vec3 past = texture2D(history, surface_texcoord).rgb;
    vec3 present = texture2D(texture, surface_texcoord - uv_jitter).rgb;
    vec3 colour = mix(present, past, 1.0 / 8.0);
    output_colour = vec4(colour, 1.0);
}
