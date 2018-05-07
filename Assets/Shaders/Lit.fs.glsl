#version 330

layout(location = 0) out vec4 output_colour;

uniform vec3 light_direction;

in vec3 surface_normal;
in vec3 surface_colour;

float half_lambert(vec3 n, vec3 l)
{
    return 0.5 * dot(n, l) + 0.5;
}

float lambert(vec3 n, vec3 l)
{
    return max(dot(n, l), 0.0);
}

void main()
{
    float light = half_lambert(surface_normal, light_direction);
    output_colour = vec4(surface_colour * vec3(light), 1.0);
}

