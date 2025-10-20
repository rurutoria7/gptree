#pragma once

#include "Records.h"
#include "Camera.h"
#include "Shading.h"

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

// Simple hash function for procedural noise
float hash(float2 p)
{
    float h = dot(p, float2(127.1, 311.7));
    return frac(sin(h) * 43758.5453123);
}

// Procedural noise function
float noise(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    f = f * f * (3.0 - 2.0 * f); // Smoothstep

    float a = hash(i);
    float b = hash(i + float2(1.0, 0.0));
    float c = hash(i + float2(0.0, 1.0));
    float d = hash(i + float2(1.0, 1.0));

    return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
}

float4 SimpleCubePixelShader(
    float3 position : POSITION0,
    bool isFrontFace : SV_IsFrontFace) : SV_TARGET
{
    // Compute normal based on which face of the cube we're on
    float3 absPos = abs(position);
    float3 normal;

    // UV coordinates for texturing (project position onto dominant axis)
    float2 uv;

    // Find which axis is closest to the boundary
    if (absPos.x > absPos.y && absPos.x > absPos.z) {
        // X face (left or right)
        normal = float3(sign(position.x), 0, 0);
        uv = position.yz;
    } else if (absPos.y > absPos.z) {
        // Y face (top or bottom)
        normal = float3(0, sign(position.y), 0);
        uv = position.xz;
    } else {
        // Z face (front or back)
        normal = float3(0, 0, sign(position.z));
        uv = position.xy;
    }

    // Procedural textures
    float2 scaledUV = uv * 8.0; // Scale for pattern frequency

    // Checkerboard pattern for roughness
    float2 checker = floor(scaledUV);
    float checkerPattern = fmod(checker.x + checker.y, 2.0);

    // Noise-based metallic pattern
    float noisePattern = noise(uv * 12.0);
    float metallicPattern = step(0.6, noisePattern); // Creates metallic spots

    // Procedural normal perturbation (subtle bumps)
    float normalNoise = noise(uv * 20.0) * 2.0 - 1.0;
    float3 tangent, bitangent;

    // Create tangent space based on face orientation
    if (abs(normal.y) > 0.9) {
        // Top/bottom face
        tangent = float3(1, 0, 0);
        bitangent = float3(0, 0, 1);
    } else if (abs(normal.x) > 0.9) {
        // Left/right face
        tangent = float3(0, 1, 0);
        bitangent = float3(0, 0, 1);
    } else {
        // Front/back face
        tangent = float3(1, 0, 0);
        bitangent = float3(0, 1, 0);
    }

    // Add subtle normal variation
    float3 perturbedNormal = normalize(normal + tangent * normalNoise * 0.1 + bitangent * normalNoise * 0.1);

    // Set up surface data for shading
    SurfaceData surface;
    surface.baseColor    = float4(1.0, 0.5, 0.0, 1.0);  // Bright orange
    surface.normal       = isFrontFace ? perturbedNormal : -perturbedNormal;
    surface.metallic     = metallicPattern * 0.8;  // Spotted metallic pattern
    surface.roughness    = lerp(0.3, 0.9, checkerPattern);  // Checkerboard roughness
    surface.occlusion    = 0.5;
    surface.translucency = 0.0;

    // Apply lighting
    return ShadeSurface(surface);
}
