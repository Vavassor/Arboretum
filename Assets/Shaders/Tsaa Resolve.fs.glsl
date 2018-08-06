// This file is subject to the MIT License:
//
// Copyright (c) [2015] [Playdead]
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// AUTHOR: Lasse Jon Fuglsang Pedersen <lasse@playdead.com>

#version 330

uniform sampler2D depth_buffer;
uniform sampler2D history;
uniform sampler2D texture;
uniform sampler2D velocity_buffer;

layout(std140) uniform TsaaResolveBlock
{
    vec2 uv_jitter;
};

in vec2 surface_texcoord;

layout(location = 0) out vec4 output_colour;

const float epsilon = 0.00000001;
const float feedback_min = 0.88;
const float feedback_max = 0.97;

float luminance(vec3 colour)
{
    return dot(colour, vec3(0.0396819152, 0.458021790, 0.00609653955));
}

vec4 clip_aabb(vec3 aabb_min, vec3 aabb_max, vec4 p, vec4 q)
{
    vec3 p_clip = 0.5 * (aabb_max + aabb_min);
    vec3 e_clip = 0.5 * (aabb_max - aabb_min) + epsilon;

    vec4 v_clip = q - vec4(p_clip, p.w);
    vec3 v_unit = v_clip.xyz / e_clip;
    vec3 a_unit = abs(v_unit);
    float ma_unit = max(a_unit.x, max(a_unit.y, a_unit.z));

    if(ma_unit > 1.0)
    {
        return vec4(p_clip, p.w) + v_clip / ma_unit;
    }
    else
    {
        return q;
    }
}

vec3 find_closest_fragment_3x3(vec2 uv)
{
    vec2 dd = abs(1.0 / textureSize(depth_buffer, 0));
    vec2 du = vec2(dd.x, 0.0);
    vec2 dv = vec2(0.0, dd.y);

    vec3 dtl = vec3(-1, -1, texture2D(depth_buffer, uv - dv - du).x);
    vec3 dtc = vec3( 0, -1, texture2D(depth_buffer, uv - dv).x);
    vec3 dtr = vec3( 1, -1, texture2D(depth_buffer, uv - dv + du).x);

    vec3 dml = vec3(-1, 0, texture2D(depth_buffer, uv - du).x);
    vec3 dmc = vec3( 0, 0, texture2D(depth_buffer, uv).x);
    vec3 dmr = vec3( 1, 0, texture2D(depth_buffer, uv + du).x);

    vec3 dbl = vec3(-1, 1, texture2D(depth_buffer, uv + dv - du).x);
    vec3 dbc = vec3( 0, 1, texture2D(depth_buffer, uv + dv).x);
    vec3 dbr = vec3( 1, 1, texture2D(depth_buffer, uv + dv + du).x);

    vec3 dmin = dtl;
    if(dmin.z > dtc.z) dmin = dtc;
    if(dmin.z > dtr.z) dmin = dtr;

    if(dmin.z > dml.z) dmin = dml;
    if(dmin.z > dmc.z) dmin = dmc;
    if(dmin.z > dmr.z) dmin = dmr;

    if(dmin.z > dbl.z) dmin = dbl;
    if(dmin.z > dbc.z) dmin = dbc;
    if(dmin.z > dbr.z) dmin = dbr;

    return vec3(uv + dd.xy * dmin.xy, dmin.z);
}

vec4 reproject_temporally(vec2 texcoord, vec2 velocity)
{
    vec4 texel0 = texture2D(texture, texcoord - uv_jitter);
    vec4 texel1 = texture2D(history, texcoord - velocity);
    
    vec2 uv = texcoord;
    
    vec2 texel_size = 1.0 / textureSize(texture, 0);
    
    vec2 du = vec2(texel_size.x, 0.0);
    vec2 dv = vec2(0.0, texel_size.y);

    vec4 ctl = texture2D(texture, uv - dv - du);
    vec4 ctc = texture2D(texture, uv - dv);
    vec4 ctr = texture2D(texture, uv - dv + du);
    vec4 cml = texture2D(texture, uv - du);
    vec4 cmc = texture2D(texture, uv);
    vec4 cmr = texture2D(texture, uv + du);
    vec4 cbl = texture2D(texture, uv + dv - du);
    vec4 cbc = texture2D(texture, uv + dv);
    vec4 cbr = texture2D(texture, uv + dv + du);

    vec4 cmin = min(ctl, min(ctc, min(ctr, min(cml, min(cmc, min(cmr, min(cbl, min(cbc, cbr))))))));
    vec4 cmax = max(ctl, max(ctc, max(ctr, max(cml, max(cmc, max(cmr, max(cbl, max(cbc, cbr))))))));

    vec4 cavg = (ctl + ctc + ctr + cml + cmc + cmr + cbl + cbc + cbr) / 9.0;

    vec4 cmin5 = min(ctc, min(cml, min(cmc, min(cmr, cbc))));
    vec4 cmax5 = max(ctc, max(cml, max(cmc, max(cmr, cbc))));
    vec4 cavg5 = (ctc + cml + cmc + cmr + cbc) / 5.0;
    cmin = 0.5 * (cmin + cmin5);
    cmax = 0.5 * (cmax + cmax5);
    cavg = 0.5 * (cavg + cavg5);

    texel1 = clip_aabb(cmin.xyz, cmax.xyz, clamp(cavg, cmin, cmax), texel1);
    
    float lum0 = luminance(texel0.rgb);
    float lum1 = luminance(texel1.rgb);
    
    float unbiased_diff = abs(lum0 - lum1) / max(lum0, max(lum1, 0.2));
    float unbiased_weight = 1.0 - unbiased_diff;
    float unbiased_weight_sqr = unbiased_weight * unbiased_weight;
    float k_feedback = mix(feedback_min, feedback_max, unbiased_weight_sqr);

    return mix(texel0, texel1, k_feedback);
}

void main()
{
    vec3 closest = find_closest_fragment_3x3(surface_texcoord);
    vec2 velocity = texture2D(velocity_buffer, closest.xy).xy;
    output_colour = reproject_temporally(surface_texcoord, velocity);
}
