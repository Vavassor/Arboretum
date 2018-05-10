#version 330

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 direction;
layout(location = 2) in vec4 colour;
layout(location = 3) in float side;

uniform mat4x4 model_view_projection;
uniform vec2 viewport;
uniform float projection_factor;
uniform float line_width;

out vec4 surface_colour;

vec4 clip_to_image_plane(vec4 f, vec4 b)
{
    float s = -b.w / (f.w - b.w);
    
    vec4 result;
    result.xyz = (s * (f.xyz - b.xyz)) + b.xyz;
    result.w = s;
    return result;
}

void main()
{
    float aspect = viewport.x / viewport.y;
    
    vec4 start = model_view_projection * vec4(position, 1.0);
    vec4 end = model_view_projection * vec4(position + direction, 1.0);
    
    // If either endpoint of the line segment is behind the camera.
    if(end.w < 0.0)
    {
        end = clip_to_image_plane(start, end);
    }
    if(start.w < 0.0)
    {
        start = clip_to_image_plane(end, start);
    }
    
    vec2 a = start.xy / start.w;
    a.x *= aspect;
    vec2 b = end.xy / end.w;
    b.x *= aspect;

    vec2 screen_direction = normalize(a - b);
    vec2 lateral = vec2(-screen_direction.y, screen_direction.x);
    lateral.x /= aspect;

    float pixel_width_ratio = 1.0 / (viewport.x * projection_factor);
    float pixel_width = start.w * pixel_width_ratio;
    lateral *= 0.5 * pixel_width * line_width;

    start.xy += lateral * side;

    gl_Position = start;

    surface_colour = colour;
}

