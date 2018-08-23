// GLSL Fragment Shader "pixel_main"
// Generated by XShaderCompiler
// 23/08/2018 15:37:34

#version 330

in vec2 xsv_TEXCOORD0;

layout(location = 0) out vec4 SV_Target0;

uniform sampler2D texture0;

layout(std140) uniform DepthBlock
{
    float near;
    float far;
};

float linearise_depth(float x, float n, float f)
{
    return (2.0f * n) / (f + n - x * (f - n));
}

void main()
{
    float depth = texture(texture0, xsv_TEXCOORD0).r;
    float value = linearise_depth(depth, near, far);
    SV_Target0 = vec4(vec3(value), 1.0);
}

