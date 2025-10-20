#pragma once

#include "Records.h"
#include "Camera.h"

struct SimpleCubeVertex
{
    float4 clipSpacePosition : SV_POSITION;
    float3 worldSpacePosition : POSITION0;
};

// Hardcoded cube geometry
// 8 vertices forming a unit cube centered at origin
static const float3 CubeVertices[] = {
    float3(-0.5, -0.5, -0.5),  // 0: back-bottom-left
    float3( 0.5, -0.5, -0.5),  // 1: back-bottom-right
    float3(-0.5,  0.5, -0.5),  // 2: back-top-left
    float3( 0.5,  0.5, -0.5),  // 3: back-top-right
    float3(-0.5, -0.5,  0.5),  // 4: front-bottom-left
    float3( 0.5, -0.5,  0.5),  // 5: front-bottom-right
    float3(-0.5,  0.5,  0.5),  // 6: front-top-left
    float3( 0.5,  0.5,  0.5),  // 7: front-top-right
};

// 12 triangles (2 per face, 6 faces)
static const uint3 SimpleCubeIndices[] = {
    // Back face (Z = -0.5)
    uint3(0, 2, 1),
    uint3(1, 2, 3),

    // Front face (Z = +0.5)
    uint3(4, 5, 6),
    uint3(5, 7, 6),

    // Left face (X = -0.5)
    uint3(0, 4, 2),
    uint3(4, 6, 2),

    // Right face (X = +0.5)
    uint3(1, 3, 5),
    uint3(3, 7, 5),

    // Bottom face (Y = -0.5)
    uint3(0, 1, 4),
    uint3(1, 5, 4),

    // Top face (Y = +0.5)
    uint3(2, 6, 3),
    uint3(3, 6, 7),
};

[Shader("node")]
[NodeLaunch("mesh")]
[NodeId("SimpleCube", 0)]
[NodeDispatchGrid(1, 1, 1)]
[NumThreads(12, 1, 1)]
[OutputTopology("triangle")]
void SimpleCubeMeshShader(
    DispatchNodeInputRecord<SimpleCubeRecord> input,

    uint groupThreadId : SV_GroupThreadID,

    out indices uint3 outputIndices[12],
    out vertices SimpleCubeVertex outputVertices[8])
{
    SetMeshOutputCounts(8, 12);

    // Output vertices
    if (groupThreadId < 8) {
        const float3 vertexPosition = CubeVertices[groupThreadId];

        outputVertices[groupThreadId].clipSpacePosition = mul(GetViewProjectionMatrix(), float4(vertexPosition, 1));
        outputVertices[groupThreadId].worldSpacePosition = vertexPosition;
    }

    // Output triangle indices
    if (groupThreadId < 12) {
        outputIndices[groupThreadId] = SimpleCubeIndices[groupThreadId];
    }
}

float4 SimpleCubePixelShader(float3 position : POSITION0) : SV_TARGET
{
    // Simple solid color - bright orange for visibility
    return float4(1.0, 0.5, 0.0, 1.0);
}
