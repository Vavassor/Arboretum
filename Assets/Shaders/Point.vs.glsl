#version 330

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 direction;
layout(location = 2) in vec4 colour;
layout(location = 3) in vec2 texcoord;

uniform float point_radius;
uniform mat4x4 model_view_projection;
uniform float projection_factor;
uniform vec2 viewport;

out vec4 surface_colour;
noperspective out vec2 surface_texcoord;

void main()
{
    float aspect = viewport.x / viewport.y;
    
    vec4 center = model_view_projection * vec4(position, 1.0);
    
    vec2 offset = direction;
    offset.x /= aspect;
    float pixel_width_ratio = 2.0 / (viewport.x * projection_factor);
    float pixel_width = center.w * pixel_width_ratio;
    float cotangent_fov_over_2 = projection_factor * aspect;
    offset *= pixel_width * point_radius * cotangent_fov_over_2;
    
    vec4 point = center;
    point.xy += offset;
    gl_Position = point;
    
    surface_texcoord = texcoord;
    surface_colour = colour;
}
