#version 330

layout(location = 0) out vec4 output_colour;
layout(location = 1) out vec2 output_velocity;

layout(std140) uniform LightBlock
{
    vec3 light_direction;
};

in vec3 surface_normal;
in vec3 surface_colour;
in vec4 clip_space_position;
in vec4 prior_clip_space_position;

float half_lambert(vec3 n, vec3 l)
{
    return 0.5 * dot(n, l) + 0.5;
}

float lambert(vec3 n, vec3 l)
{
    return max(dot(n, l), 0.0);
}

vec2 get_velocity(vec4 now, vec4 prior)
{
    vec3 ndc_now = (now / now.w).xyz;
    vec3 ndc_prior = (prior / prior.w).xyz;
    return (ndc_now - ndc_prior).xy;
}

void main()
{
    float light = half_lambert(surface_normal, light_direction);
    output_colour = vec4(surface_colour * vec3(light), 1.0);
    output_velocity = get_velocity(clip_space_position, prior_clip_space_position);
}
