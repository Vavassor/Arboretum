// The code in this file is adapted from:
//
//                     NVIDIA FXAA 3.11 by TIMOTHY LOTTES
//
// ------------------------------------------------------------------------------
// COPYRIGHT (C) 2010, 2011 NVIDIA CORPORATION. ALL RIGHTS RESERVED.
// ------------------------------------------------------------------------------
// TO THE MAXIMUM EXTENT PERMITTED BY APPLICABLE LAW, THIS SOFTWARE IS PROVIDED
// *AS IS* AND NVIDIA AND ITS SUPPLIERS DISCLAIM ALL WARRANTIES, EITHER EXPRESS
// OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL NVIDIA
// OR ITS SUPPLIERS BE LIABLE FOR ANY SPECIAL, INCIDENTAL, INDIRECT, OR
// CONSEQUENTIAL DAMAGES WHATSOEVER (INCLUDING, WITHOUT LIMITATION, DAMAGES FOR
// LOSS OF BUSINESS PROFITS, BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION,
// OR ANY OTHER PECUNIARY LOSS) ARISING OUT OF THE USE OF OR INABILITY TO USE
// THIS SOFTWARE, EVEN IF NVIDIA HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
// DAMAGES.

#version 330

uniform sampler2D frame_colour;

in vec2 surface_texcoord;

layout(location = 0) out vec4 output_colour;

layout(std140) uniform FxaaBlock
{
    vec2 rcp_frame_option_1;
    vec2 rcp_frame_option_2;
    float edge_sharpness;
    float edge_threshold;
    float edge_threshold_min;
};

float luminance(vec3 colour)
{
    return dot(colour, vec3(0.299, 0.587, 0.114));
}

vec4 fxaa(vec2 texcoord)
{
    vec3 rgbNW = textureOffset(frame_colour, texcoord, ivec2(-1, 1)).rgb;
    vec3 rgbNE = textureOffset(frame_colour, texcoord, ivec2(1, 1)).rgb;
    vec3 rgbSW = textureOffset(frame_colour, texcoord, ivec2(-1, -1)).rgb;
    vec3 rgbSE = textureOffset(frame_colour, texcoord, ivec2(1, -1)).rgb;
    vec3 rgbM = texture(frame_colour, texcoord).rgb;
    
    float lumaNw = luminance(rgbNW);
    float lumaNe = luminance(rgbNE);
    float lumaSw = luminance(rgbSW);
    float lumaSe = luminance(rgbSE);
    float lumaM = luminance(rgbM);

    float lumaMaxNwSw = max(lumaNw, lumaSw);
    lumaNe += 1.0 / 384.0;
    float lumaMinNwSw = min(lumaNw, lumaSw);

    float lumaMaxNeSe = max(lumaNe, lumaSe);
    float lumaMinNeSe = min(lumaNe, lumaSe);

    float lumaMax = max(lumaMaxNeSe, lumaMaxNwSw);
    float lumaMin = min(lumaMinNeSe, lumaMinNwSw);

    float lumaMaxScaled = lumaMax * edge_threshold;

    float lumaMinM = min(lumaMin, lumaM);
    float lumaMaxScaledClamped = max(edge_threshold_min, lumaMaxScaled);
    float lumaMaxM = max(lumaMax, lumaM);
    float dirSwMinusNe = lumaSw - lumaNe;
    float lumaMaxSubMinM = lumaMaxM - lumaMinM;
    float dirSeMinusNw = lumaSe - lumaNw;
    if(lumaMaxSubMinM < lumaMaxScaledClamped)
    {
        return vec4(rgbM, 1.0);
    }

    vec2 dir;
    dir.x = dirSwMinusNe + dirSeMinusNw;
    dir.y = dirSwMinusNe - dirSeMinusNw;

    vec2 dir1 = normalize(dir.xy);
    vec4 rgbyN1 = texture(frame_colour, texcoord - dir1 * rcp_frame_option_1);
    vec4 rgbyP1 = texture(frame_colour, texcoord + dir1 * rcp_frame_option_1);
    
    float dirAbsMinTimesC = min(abs(dir1.x), abs(dir1.y)) * edge_sharpness;
    vec2 dir2 = clamp(dir1.xy / dirAbsMinTimesC, -2.0, 2.0);
    
    vec4 rgbyN2 = texture(frame_colour, texcoord - dir2 * rcp_frame_option_2);
    vec4 rgbyP2 = texture(frame_colour, texcoord + dir2 * rcp_frame_option_2);
    
    vec4 rgbyA = rgbyN1 + rgbyP1;
    vec4 rgbyB = ((rgbyN2 + rgbyP2) * 0.25) + (rgbyA * 0.25);
    
    if((rgbyB.w < lumaMin) || (rgbyB.w > lumaMax))
    {
        rgbyB.xyz = rgbyA.xyz * 0.5;
    }
    
    return rgbyB;
}

void main()
{
    output_colour = fxaa(surface_texcoord);
}
