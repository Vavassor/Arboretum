#version 330

layout(location = 0) out vec4 output_colour;

layout(std140) uniform HaloBlock
{
    vec4 halo_colour;
};

void main()
{
    output_colour = halo_colour;
}

