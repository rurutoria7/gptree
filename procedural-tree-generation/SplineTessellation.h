#pragma once

#include "Camera.h"

/*
    *----t----*
    |----u----|
    |         |
    v         v  u = (f + t)/2
    |         |
    |----u----|
    *----f----*
*/
static const int minPoints  = 2;
static const int maxPointsU = min(maxNumVerticesPerGroup / 2, maxNumTrianglesPerGroup / 2 + 1);
static const int maxPointsV = 128;

struct SegmentTessellationData {
    uint  threadGroupCount;
    float fromPoints;
    float toPoints;
    float vPoints;
    int faceRingsPerGroup;
    float fromOpeningAngle;
    float toOpeningAngle;
};

float3 ArbitraryOrthonormal(in const float3 n)
{
    float s = Sign(n.z);
    const float a = -1.0f / (s + n.z);
    const float b = n.x * n.y * a;
    return float3(b, s + n.y * n.y * a, -n.y);
}

float3 PosToClip(in const float4x4 viewProjectionMatrix, in const float3 pos){
    float4 ret = mul(viewProjectionMatrix, float4(pos, 1));
    return ret.xyz / ret.w;
}

float3 PosToPixel(in const float4x4 viewProjectionMatrix, in const float3 pos){
    return PosToClip(viewProjectionMatrix, pos) * float3(RenderSize * .5, 1);
}

float GetOpeningAngle(in const float3 toCam, float3 up, float z, float radius_){
    if(z >= 0.95) return PI;
    float d = dot(toCam, up);
    float tan_alpha = d / sqrt(1. - d*d);
    float a = tan_alpha * radius_;
    float openingAngle = (a < -1) ? PI : acos(a);
    return clamp(openingAngle, 0. * PI, PI);
}


SegmentTessellationData ComputeVisibilityAndTessellationData(
    in  const SegmentInfo    si,
    in  const TreeParameters params,
    in  const TreeTransform  cageFrom,
    in  const TreeTransform  cageTo,
    in  const float          pixelsPerTriangle
)
{
    SegmentTessellationData result = (SegmentTessellationData)0;
    result.threadGroupCount        = 0;

    const float3   cameraPosition       = GetCameraPosition();
    const float4x4 viewProjectionMatrix = GetViewProjectionMatrix();

    // Estimate resolution of stem segment
    const float3 fromPos2cam = normalize(cameraPosition - cageFrom.pos);
    const float3 toPos2cam   = normalize(cameraPosition - cageTo.pos);

    const float3 fromPerpVector = normalize(ArbitraryOrthonormal(fromPos2cam));
    const float3 toPerpVector   = normalize(ArbitraryOrthonormal(toPos2cam));

    const float2 fromPixel = PosToPixel(viewProjectionMatrix, cageFrom.pos).xy;
    const float2 toPixel   = PosToPixel(viewProjectionMatrix, cageTo.pos).xy;

    float fromZ = si.GetFromZ();
    float toZ   = si.GetToZ();

    float fromRingRadius = GetTaperedRadius(si, params, fromZ);
    float toRingRadius   = GetTaperedRadius(si, params, toZ);

    const float2 fromPixelB = PosToPixel(viewProjectionMatrix, cageFrom.pos + fromPerpVector * fromRingRadius).xy;
    const float2 toPixelB   = PosToPixel(viewProjectionMatrix, cageTo.pos   + toPerpVector   * toRingRadius).xy;

    float fromPixelDiameter = 2 * distance(fromPixel, fromPixelB);
    float toPixelDiameter   = 2 * distance(toPixel  , toPixelB);

    float lengthPixel = distance(fromPixel, toPixel);

    // Small contribution culling
    const float minBranchPixelRadius = 1;
    const uint3 sip                  = PackSegmentInfo(si);
    const bool  draw                 = (max(fromPixelDiameter, toPixelDiameter) > (minBranchPixelRadius * random::Random(sip.x, sip.y, sip.z, 1337)));

    // Frustum culling using bounding sphere
    const float3 segmentCenter = (cageFrom.pos + cageTo.pos) * 0.5;
    const float segmentLength = distance(cageFrom.pos, cageTo.pos);
    const float maxRadius = max(fromRingRadius, toRingRadius);
    const float boundingSphereRadius = sqrt(maxRadius * maxRadius + (segmentLength * 0.5) * (segmentLength * 0.5));

    const Frustum frustum = GetViewFrustum();
    const bool inFrustum = SphereInFrustum(frustum, segmentCenter, boundingSphereRadius);

    if (!draw || !inFrustum) {
        return result;
    }

    static const float h = 0.01;
    const float fromRingRadius_ = (GetTaperedRadius(si, params, fromZ + h) - fromRingRadius) / (h * si.length);
    const float toRingRadius_   = (GetTaperedRadius(si, params, toZ   + h) -   toRingRadius) / (h * si.length);

    const float fromOpeningAngle  = GetOpeningAngle(fromPos2cam, qGetZ(cageFrom.rot), fromZ, fromRingRadius_);
    const float toOpeningAngle    = GetOpeningAngle(toPos2cam,   qGetZ(cageTo.rot),   toZ,   toRingRadius_);

    result.fromPoints = (fromOpeningAngle * fromPixelDiameter) / pixelsPerTriangle;
    result.toPoints   = (toOpeningAngle   * toPixelDiameter) / pixelsPerTriangle;
    result.vPoints    = lengthPixel / pixelsPerTriangle;

    result.fromPoints = clamp(result.fromPoints, minPoints, maxPointsU);
    result.toPoints   = clamp(result.toPoints,   minPoints, maxPointsU);
    result.vPoints    = clamp(result.vPoints,    minPoints, maxPointsV);

    int fromPointsI = RoundUpMultiple2(result.fromPoints);
    int toPointsI   = RoundUpMultiple2(result.toPoints);
    float uPoints   = lerp(result.fromPoints, result.toPoints, .5);
    int uPointsI    = RoundUpMultiple2(uPoints);
    int vPointsI    = RoundUpMultiple2(result.vPoints);

    int biggestRingV = max(fromPointsI, toPointsI) + uPointsI;
    int biggestRingT = biggestRingV - 2;

    int maxRingsPerGroupV = 1 + (maxNumVerticesPerGroup  - biggestRingV) / uPointsI;
    int maxRingsPerGroupT = 1 + (maxNumTrianglesPerGroup - biggestRingT) / (2 * (uPointsI - 1));

    result.faceRingsPerGroup = min(vPointsI - 1, min(maxRingsPerGroupV, maxRingsPerGroupT));
    result.threadGroupCount = (vPointsI + result.faceRingsPerGroup - 2) / result.faceRingsPerGroup;

    result.fromOpeningAngle = fromOpeningAngle;
    result.toOpeningAngle   = toOpeningAngle;

    result.fromOpeningAngle = (fromPointsI < 5) ? .5 * PI : fromOpeningAngle;
    result.toOpeningAngle   = (toPointsI < 5)   ? .5 * PI : toOpeningAngle;

    return result;
}