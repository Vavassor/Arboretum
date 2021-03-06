// GLSL Fragment Shader "pixel_main"
// Generated by XShaderCompiler
// 23/08/2018 15:37:34

#version 330

in vec2 xsv_TEXCOORD0;

layout(location = 0) out vec4 SV_Target0;

uniform sampler2D texture0;

layout(std140) uniform PerSpan
{
    vec3 tint;
};

void main()
{
    vec4 xst_colour = texture(texture0, xsv_TEXCOORD0);
    xst_colour.rgb *= tint;
    SV_Target0 = xst_colour;
}

