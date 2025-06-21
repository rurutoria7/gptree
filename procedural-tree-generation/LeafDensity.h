#pragma once

#include "Common.h"

// ============================ Leaf Density Functions ====================

float ComputeChildScale(in float childDensity)
{
    return clamp(1.f / sqrt(childDensity), 1.f, 5.f);
}

float ComputeChildDensity(in const float distanceToCamera)
{
#if USING_SOFTWARE_ADAPTER
    // Use lower tree density settings for WARP software adapter
    const float densityFactor       = 0.9f;
    const float densityHalfDistance = 30.f;
#else
    const float densityFactor       = 1.f;
    const float densityHalfDistance = 60.f;
#endif

    return clamp(densityFactor * pow(2, -distanceToCamera / densityHalfDistance), 0.0001, 1);
}

void ComputeChildDensityAndScale(in const float distanceToCamera, out float childDensity, out float childScale)
{
    childDensity = ComputeChildDensity(distanceToCamera);
    childScale   = ComputeChildScale(childDensity);
}

float GetChildScale(in float groupScale, in float elementScale)
{
    if (groupScale == 1.f) {
        return elementScale != 0.f? 1.f : 0.f;
    } else {
        if (elementScale == 0.f) {
            return 0.f;
        } else if (elementScale == 1.f) {
            return 1.f;
        } else {
            return groupScale;
        }
    }
}

uint GetChildScaleGroupSize(in uint childCount) {
    return childCount > 64? 16 : 4;
}

float GetChildScaleFromIndex(in uint childIndex, in uint childCount, in float childDensity)
{
    const uint groupSize  = GetChildScaleGroupSize(childCount);
    const uint groupCount = DivideAndRoundUp(childCount, groupSize);

    const uint  virtualChildCount   = groupSize * groupCount;
    const float virtualChildDensity = (childCount * childDensity) / float(virtualChildCount);

    const uint smallGroupCount = virtualChildCount - childCount;
    const uint smallGroupSkip  = min(childIndex / (groupSize - 1), smallGroupCount);
    const uint k = childIndex - (smallGroupSkip * (groupSize - 1));

    const uint groupIndex   = groupCount - 1 - (smallGroupSkip + (k / groupSize));
    const uint elementIndex = (k % groupSize);

    const float d = virtualChildDensity == 1.f? 1.f : ((virtualChildDensity * groupSize) % 1.f);

    const float groupScale   = clamp(((d * groupCount)) - (groupIndex), 0.f, 1.f);
    const float elementScale = clamp((virtualChildDensity * groupSize) - elementIndex, 0.f, 1.f);

    if (groupIndex >= groupCount) {
        return 0;
    }

    if ((groupIndex == 0) && (elementIndex == 0)) {
        // always keep at least one child
        return 1.f;
    }

    return GetChildScale(groupScale, elementScale);
}

void GetNthChildIndexAndScale(in uint n, in uint childCount, in float childDensity, out uint childIndex, out float childScale)
{
    const uint groupSize  = GetChildScaleGroupSize(childCount);
    const uint groupCount = DivideAndRoundUp(childCount, groupSize);

    const uint aliveChildCount = ceil(childCount * childDensity);

    if (n >= aliveChildCount) {
        childIndex = 0;
        childScale = 0.f;
        return;
    }

    const uint  virtualChildCount   = groupSize * groupCount;
    const float virtualChildDensity = (childCount * childDensity) / float(virtualChildCount);

    const uint childrenPerGroupF = max(floor(groupSize * virtualChildDensity), 1);
    const uint childrenPerGroupC = max(ceil(groupSize * virtualChildDensity), 1);

    const float d = ((virtualChildDensity * groupSize) % 1.f) == 0? 1.f : ((virtualChildDensity * groupSize) % 1.f);

    // Small groups to match child count
    const uint smallGroupCount = virtualChildCount - childCount;

    const uint groupCountC = ceil(d * groupCount);
    const uint groupCountF = groupCount - groupCountC;

    const uint groupSkipF = min((n / childrenPerGroupF), groupCountF);
    const uint groupSkipC = max(int(n) - int(groupSkipF * childrenPerGroupF), 0) / childrenPerGroupC;
    uint groupIndex = groupSkipF + groupSkipC;

    uint elementIndex = max(int(n) - int(groupSkipF * childrenPerGroupF), 0) % childrenPerGroupC;

    if (aliveChildCount <= groupCount) {
        groupIndex = groupCount - 1 - groupIndex;
    }

    const float groupScale   = clamp(((d * groupCount)) - (groupCount - 1 - groupIndex), 0.f, 1.f);
    const float elementScale = clamp((virtualChildDensity * groupSize) - elementIndex, 0.f, 1.f);

    const uint smallGroupSkip = min(groupIndex, smallGroupCount);
    childIndex = smallGroupSkip * (groupSize - 1) + (groupIndex - smallGroupSkip) * groupSize + elementIndex;
    childScale = GetChildScale(groupScale, elementScale);

    if (childIndex >= childCount) {
        childIndex = 0;
        childScale = 0.f;
    }
    if (n == 0) {
        // Always keep at least one child
        childScale = 1.f;
    }
}