#ifndef PER_VIEW_H_
#define PER_VIEW_H_

cbuffer PerView
{
    float4x4 view_projection;
    float2 viewport_dimensions;
};

#endif // PER_VIEW_H_

