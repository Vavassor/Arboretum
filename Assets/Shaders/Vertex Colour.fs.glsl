#version 330

layout(location = 0) out vec4 output_colour;
layout(location = 1) out vec2 output_velocity;

in vec4 surface_colour;
in vec4 clip_space_position;
in vec4 prior_clip_space_position;

vec2 get_velocity(vec4 now, vec4 prior)
{
    vec3 ndc_now = (now / now.w).xyz;
    vec3 ndc_prior = (prior / prior.w).xyz;
    return 0.5 * (ndc_now - ndc_prior).xy;
}

void main()
{
    output_colour = surface_colour;
    output_velocity = get_velocity(clip_space_position, prior_clip_space_position);
}

