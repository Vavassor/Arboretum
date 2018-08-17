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

Texture2D frame_colour;
SamplerState sampler0;

cbuffer FxaaBlock
{
    float2 rcp_frame_option_1;
    float2 rcp_frame_option_2;
    float edge_sharpness;
    float edge_threshold;
    float edge_threshold_min;
};

struct PixelInput
{
    float2 texcoord : TEXCOORD;
};

struct PixelOutput
{
    float4 colour : SV_TARGET;
};

float luminance(float3 colour)
{
    return dot(colour, float3(0.299, 0.587, 0.114));
}

float4 fxaa(float2 texcoord)
{
    float3 rgbNW = frame_colour.Sample(sampler0, texcoord, int2(-1, 1)).rgb;
    float3 rgbNE = frame_colour.Sample(sampler0, texcoord, int2(1, 1)).rgb;
    float3 rgbSW = frame_colour.Sample(sampler0, texcoord, int2(-1, -1)).rgb;
    float3 rgbSE = frame_colour.Sample(sampler0, texcoord, int2(1, -1)).rgb;
    float3 rgbM = frame_colour.Sample(sampler0, texcoord).rgb;
    
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
        return float4(rgbM, 1.0);
    }

    float2 dir;
    dir.x = dirSwMinusNe + dirSeMinusNw;
    dir.y = dirSwMinusNe - dirSeMinusNw;

    float2 dir1 = normalize(dir.xy);
    float4 rgbyN1 = frame_colour.Sample(sampler0, texcoord - dir1 * rcp_frame_option_1);
    float4 rgbyP1 = frame_colour.Sample(sampler0, texcoord + dir1 * rcp_frame_option_1);
    
    float dirAbsMinTimesC = min(abs(dir1.x), abs(dir1.y)) * edge_sharpness;
    float2 dir2 = clamp(dir1.xy / dirAbsMinTimesC, -2.0, 2.0);
    
    float4 rgbyN2 = frame_colour.Sample(sampler0, texcoord - dir2 * rcp_frame_option_2);
    float4 rgbyP2 = frame_colour.Sample(sampler0, texcoord + dir2 * rcp_frame_option_2);
    
    float4 rgbyA = rgbyN1 + rgbyP1;
    float4 rgbyB = ((rgbyN2 + rgbyP2) * 0.25) + (rgbyA * 0.25);
    
    if((rgbyB.w < lumaMin) || (rgbyB.w > lumaMax))
    {
        rgbyB.xyz = rgbyA.xyz * 0.5;
    }
    
    return rgbyB;
}

PixelOutput pixel_main(in PixelInput input)
{
    PixelOutput output;
    output.colour = fxaa(input.texcoord);
    return output;
}
