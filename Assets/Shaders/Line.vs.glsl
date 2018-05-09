#version 330

layout(location = 0) in vec3 start;
layout(location = 1) in vec3 end;
layout(location = 2) in vec4 colour;
layout(location = 3) in float side;

uniform mat4x4 model_view_projection;
uniform vec2 viewport;
uniform float projection;
uniform float line_width;

out vec4 surface_colour;

void main()
{
    float aspect = viewport.x / viewport.y;

    vec4 a_projected = model_view_projection * vec4(start, 1.0);
    vec2 a = a_projected.xy / a_projected.w;
    a.x *= aspect;

    vec4 b_projected = model_view_projection * vec4(end, 1.0);
    vec2 b = b_projected.xy / b_projected.w;
    b.x *= aspect;

    vec2 direction = normalize(a - b);
    vec2 lateral = vec2(-direction.y, direction.x);
    lateral.x /= aspect;

    float pixel_width_ratio = 1.0 / (viewport.x * projection);
    float pixel_width = a_projected.w * pixel_width_ratio;
    lateral *= 0.5 * pixel_width * line_width;

    a_projected.xy += lateral * side;

    gl_Position = a_projected;

    surface_colour = colour;
}

