#pragma once

float3 Normal(float3 pos)
{
    return normalize(cross(ddy_fine(pos), ddx_fine(pos)));
}

struct SurfaceData {
    float4 baseColor;
    float3 normal;
    float metallic;
    float roughness;
    float occlusion;
    float translucency;
};

static const float3 LightDirection = normalize(float3(1, 1, 1));
static const float3 LightColor     = float3(2, 1.8, 1.8);

float4 ShadeSurface(in const SurfaceData surface)
{
    float3 light = saturate(dot(LightDirection, surface.normal)) * LightColor;

    // handle translucency
    light += saturate(dot(LightDirection, -surface.normal)) * LightColor * surface.translucency;

    float3 color = 0;

    // ambient
    color  += 2 * surface.baseColor.rgb * pow(surface.occlusion, 2);
    
    // direct illumination
    color += surface.baseColor.rgb * light;

    return float4(color, surface.baseColor.a);
}