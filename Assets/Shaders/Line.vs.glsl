#version 330

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 direction;
layout(location = 2) in vec4 colour;
layout(location = 3) in vec2 texcoord;
layout(location = 4) in float side;

layout(std140) uniform PerImage
{
    vec2 texture_dimensions;
};

layout(std140) uniform PerLine
{
    float line_width;
    float projection_factor;
};

layout(std140) uniform PerObject
{
    mat4x4 model;
    mat4x4 normal_matrix;
};

layout(std140) uniform PerView
{
    mat4x4 view_projection;
    vec2 viewport_dimensions;
};

out vec4 surface_colour;
noperspective out vec2 surface_texcoord;

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
    float aspect = viewport_dimensions.x / viewport_dimensions.y;
    
    mat4x4 model_view_projection = view_projection * model;
    
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

    float pixel_width_ratio = 2.0 / (viewport_dimensions.x * projection_factor);
    float pixel_width = start.w * pixel_width_ratio;
    float cotangent_fov_over_2 = projection_factor * aspect;
    lateral *= 0.5 * pixel_width * line_width * cotangent_fov_over_2;
    
    start.xy += lateral * side;

    gl_Position = start;
    
    float texture_aspect = texture_dimensions.x / texture_dimensions.y;
    
    float screen_distance = distance(viewport_dimensions.y * a, viewport_dimensions.y * b);
    float texel_distance = screen_distance * texture_aspect / line_width;
    float texcoord_scale = 0.5 * texel_distance;
    
    surface_texcoord = vec2(texcoord.x, texcoord.y * texcoord_scale);
    surface_colour = colour;
}

