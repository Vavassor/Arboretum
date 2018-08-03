#version 330

uniform sampler2D texture;

layout(std140) uniform DepthBlock
{
    float near;
    float far;
};

layout(location = 0) out vec4 output_colour;

in vec2 surface_texcoord;

float linearise_depth(float x, float n, float f)
{
    return (2.0 * n) / (f + n - x * (f - n));
}

void main()
{
    float sample = texture2D(texture, surface_texcoord).r;
    float depth = linearise_depth(sample, near, far);
    output_colour = vec4(vec3(depth), 1.0);
}
