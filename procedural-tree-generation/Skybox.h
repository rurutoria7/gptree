#pragma once

#include "Records.h"
#include "Camera.h"
#include "Shading.h"

struct SkyboxVertex
{
    float4 clipSpacePosition : SV_POSITION;
    float3 worldSpacePosition : POSITION0;
};

struct SkyboxPrimitive
{
    uint type : BLENDINDICES0;
};

static uint4 CubeTriangles[] = {
    uint4(0, 1, 2, 0),
    uint4(3, 2, 1, 0),

    uint4(1, 5, 3, 0),
    uint4(7, 3, 5, 0),

    uint4(4, 0, 6, 0),
    uint4(2, 6, 0, 0),

    uint4(5, 4, 7, 0),
    uint4(6, 7, 4, 0),

    uint4(2, 3, 6, 0),
    uint4(7, 6, 3, 0),

    uint4(4, 5, 0, 0),
    uint4(1, 0, 5, 0),

    uint4(10,  9, 8, 1),
    uint4(10, 11, 9, 1),
};

static const float DiskRadius = 2.f;

[Shader("node")]
[NodeLaunch("mesh")]
[NodeId("Skybox", 0)]
[NodeDispatchGrid(1, 1, 1)]
[NumThreads(14, 1, 1)]
[OutputTopology("triangle")]
void SkyboxMeshShader(
    DispatchNodeInputRecord<SkyboxRecord> input,

    uint groupThreadId : SV_GroupThreadID,

    out indices    uint3           outputIndices[14],
    out primitives SkyboxPrimitive outputPrimitives[14],
    out vertices   SkyboxVertex    outputVertices[12])
{
    SetMeshOutputCounts(12, 14);

    if (groupThreadId < 12) {
        const float yMin  = IsBitSet(groupThreadId, 3)? 0.f        : -1.f;
        const float scale = IsBitSet(groupThreadId, 3)? DiskRadius :  100.f;

        const float3 boundingBoxMin = scale * float3(-1.f, yMin, -1.f);
        const float3 boundingBoxMax = scale * float3( 1.f,  1.f,  1.f);

        const float3 vertexPosition =
            float3(IsBitSet(groupThreadId, 0) ? boundingBoxMax.x : boundingBoxMin.x,
                   IsBitSet(groupThreadId, 2) ? boundingBoxMax.y : boundingBoxMin.y,
                   IsBitSet(groupThreadId, 1) ? boundingBoxMax.z : boundingBoxMin.z);

        outputVertices[groupThreadId].clipSpacePosition  = mul(GetViewProjectionMatrix(), float4(vertexPosition, 1));
        outputVertices[groupThreadId].worldSpacePosition = vertexPosition;
    }

    if (groupThreadId < 14) {
        outputIndices[groupThreadId]         = CubeTriangles[groupThreadId].xyz;
        outputPrimitives[groupThreadId].type = CubeTriangles[groupThreadId].w;
    }
}

float4 SkyboxPixelShader(
    float3 position : POSITION0, 
    uint type: BLENDINDICES0,
    bool isFrontFace : SV_IsFrontFace) : SV_TARGET
{
    // Shade disk surface
    if (type != 0) {
        const float distanceToOrigin = length(position);
        if (distanceToOrigin > DiskRadius) {
            discard;
        }

        SurfaceData surface;
        surface.baseColor    = float4(0.3, 0.3, 0.3, 1);
        surface.normal       = float3(0, isFrontFace? 1 : -1, 0);
        surface.metallic     = 0;
        surface.roughness    = 0.8;
        surface.occlusion    = 1;
        surface.translucency = 0;

        return ShadeSurface(surface);
    }

    // Compute view direction
    const float3 view = normalize(position - GetCameraPosition());

    const float3 color = float3(0.4, 0.7, 1.0) * (view.y + 1.5) * 0.5;
    const float3 light = saturate(dot(LightDirection, view)) * 0.2;
    const float3 sun   = pow(MapRange(dot(LightDirection, view), 0.95f, 1.f, 0.f, 1.f), 8) * LightColor;
    return float4(color + light + sun, 1);
}