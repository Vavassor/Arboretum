// GLSL Fragment Shader "pixel_main"
// Generated by XShaderCompiler
// 21/08/2018 16:48:12

#version 330

in vec2 xsv_TEXCOORD0;

layout(location = 0) out vec4 SV_Target0;

uniform sampler2D texture0;

void main()
{
    vec2 value = texture(texture0, xsv_TEXCOORD0).rg;
    vec2 xst_colour = (value + vec2(1.0)) * 0.5f;
    SV_Target0 = vec4(xst_colour.r, xst_colour.g, 0.0, 1.0);
}

