#version 330

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 direction;
layout(location = 2) in vec4 colour;
layout(location = 3) in vec2 texcoord;

layout(std140) uniform PerObject
{
    mat4x4 model;
    mat4x4 normal_matrix;
};

layout(std140) uniform PerPass
{
    mat4x4 view_projection;
    vec2 viewport_dimensions;
};

layout(std140) uniform PerPoint
{
    float point_radius;
    float projection_factor;
};

out vec4 surface_colour;
noperspective out vec2 surface_texcoord;

void main()
{
    float aspect = viewport_dimensions.x / viewport_dimensions.y;
    
    vec4 center = view_projection * model * vec4(position, 1.0);
    
    vec2 offset = direction;
    offset.x /= aspect;
    float pixel_width_ratio = 2.0 / (viewport_dimensions.x * projection_factor);
    float pixel_width = center.w * pixel_width_ratio;
    float cotangent_fov_over_2 = projection_factor * aspect;
    offset *= pixel_width * point_radius * cotangent_fov_over_2;
    
    vec4 point = center;
    point.xy += offset;
    gl_Position = point;
    
    surface_texcoord = texcoord;
    surface_colour = colour;
}
