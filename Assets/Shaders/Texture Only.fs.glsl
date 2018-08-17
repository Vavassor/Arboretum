// GLSL Fragment Shader "pixel_main"
// Generated by XShaderCompiler
// 16/08/2018 23:04:15

#version 330

in vec2 xsv_TEXCOORD0;

layout(location = 0) out vec4 SV_Target0;

uniform sampler2D texture0;

void main()
{
    SV_Target0 = vec4(texture(texture0, xsv_TEXCOORD0).rgb, 1.0);
}

