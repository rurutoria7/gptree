#pragma once

#include "TreeParameters.h"
#include "Config.h"

struct CameraParameters {
    float3 position;
    float3 focus;
};

// Returns camera rotating around the tree
CameraParameters GetCameraParameters() {
    const TreeParameters params = GetTreeParameters();
    const float treeHeight = params.Scale * params.nLength[0];

    const float yaw      = radians(LoadPersistentConfigFloat(PersistentConfig::CAMERA_YAW));
    const float pitch    = radians(LoadPersistentConfigFloat(PersistentConfig::CAMERA_PITCH));
    const float distance = LoadPersistentConfigFloat(PersistentConfig::CAMERA_DISTANCE);

    const float3 dir = normalize(float3(cos(yaw) * cos(pitch), sin(pitch), sin(yaw) * cos(pitch)));

    CameraParameters cameraParams;
    cameraParams.focus    = float3(0, treeHeight / 2.f, 0);
    cameraParams.position = cameraParams.focus + dir * distance;

    return cameraParams;
}

float3 GetCameraPosition()
{
    return GetCameraParameters().position;
}

// Returns view-projection matrix for the camera looking at the tree
float4x4 GetViewProjectionMatrix()
{
    const CameraParameters params = GetCameraParameters();
    const float3 forward          = normalize(params.position - params.focus);
    const float3 right            = normalize(cross(float3(0, 1, 0), forward));
    const float3 up               = normalize(cross(forward, right));

    const float3 translation = float3(
        dot(params.position, right),
        dot(params.position, up),
        dot(params.position, forward)
    );

    const float4x4 viewMatrix = float4x4(
        right.x, right.y, right.z, -translation.x,
        up.x, up.y, up.z, -translation.y,
        forward.x, forward.y, forward.z, -translation.z,
        0, 0, 0, 1
    );

    const float aspectRatio = RenderSize.y / float(RenderSize.x);

    const float4x4 projectionMatrix = float4x4(
        1.7320509 * aspectRatio, 0, 0, 0,
        0, 1.7320509, 0, 0,
        0, 0, -1.0001, -0.10001,
        0, 0, -1, 0
    );

    return mul(projectionMatrix, viewMatrix);
}

// Frustum plane in Hessian normal form: ax + by + cz + d = 0
struct FrustumPlane {
    float3 normal;  // (a, b, c)
    float d;        // distance
};

struct Frustum {
    FrustumPlane planes[6];  // Left, Right, Bottom, Top, Near, Far
};

// Extract frustum planes from view-projection matrix
// Planes are in world space and normalized
// Uses Gribb-Hartmann method for row-major matrices
Frustum GetViewFrustum() {
    float4x4 vp = GetViewProjectionMatrix();

    // Extract rows from matrix
    float4 row0 = vp[0];  // [m00, m01, m02, m03]
    float4 row1 = vp[1];  // [m10, m11, m12, m13]
    float4 row2 = vp[2];  // [m20, m21, m22, m23]
    float4 row3 = vp[3];  // [m30, m31, m32, m33]

    Frustum frustum;

    // Left plane = row3 + row0
    float4 leftPlane = row3 + row0;
    float leftLen = length(leftPlane.xyz);
    frustum.planes[0].normal = leftPlane.xyz / leftLen;
    frustum.planes[0].d = leftPlane.w / leftLen;

    // Right plane = row3 - row0
    float4 rightPlane = row3 - row0;
    float rightLen = length(rightPlane.xyz);
    frustum.planes[1].normal = rightPlane.xyz / rightLen;
    frustum.planes[1].d = rightPlane.w / rightLen;

    // Bottom plane = row3 + row1
    float4 bottomPlane = row3 + row1;
    float bottomLen = length(bottomPlane.xyz);
    frustum.planes[2].normal = bottomPlane.xyz / bottomLen;
    frustum.planes[2].d = bottomPlane.w / bottomLen;

    // Top plane = row3 - row1
    float4 topPlane = row3 - row1;
    float topLen = length(topPlane.xyz);
    frustum.planes[3].normal = topPlane.xyz / topLen;
    frustum.planes[3].d = topPlane.w / topLen;

    // Near plane = row2
    float4 nearPlane = row2;
    float nearLen = length(nearPlane.xyz);
    frustum.planes[4].normal = nearPlane.xyz / nearLen;
    frustum.planes[4].d = nearPlane.w / nearLen;

    // Far plane = row3 - row2
    float4 farPlane = row3 - row2;
    float farLen = length(farPlane.xyz);
    frustum.planes[5].normal = farPlane.xyz / farLen;
    frustum.planes[5].d = farPlane.w / farLen;

    return frustum;
}

// Test if sphere intersects frustum
// Returns true if sphere is partially or fully inside frustum
// Returns false if sphere is completely outside frustum
bool SphereInFrustum(in Frustum frustum, in float3 center, in float radius) {
    for (int i = 0; i < 6; ++i) {
        float distance = dot(frustum.planes[i].normal, center) + frustum.planes[i].d;
        if (distance < -radius) {
            return false;  // Sphere completely outside this plane
        }
    }
    return true;  // Sphere intersects or inside frustum
}