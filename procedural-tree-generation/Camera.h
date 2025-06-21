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