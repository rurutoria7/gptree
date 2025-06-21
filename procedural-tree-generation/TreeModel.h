#pragma once

#include "TreeParameters.h"
#include "Quaternion.h"
#include "Records.h"

// Functions with a _ suffix are derivatives of the original functions.
// Functions with a __ suffix are second derivatives of the original functions.

template <typename T>
T Bezier(T v0, T v1, T v2, float t){
    return (1 - t) * (1 - t) * v0 + 2 * (1 - t) * t * v1 + t * t * v2;
}

template <typename T>
T Bezier_(T v0, T v1, T v2, float t){
    return 2 * (1. - t) * (v1 - v0) + 2. * t * (v2 - v1);
}

template <typename T>
T Bezier__(T v0, T v1, T v2, float t){
    return 2 * (v2 - 2*v1 + v0);
}

float ShapeRatio(int shape, float ratio){
    /*conical*/             if(shape == 0) return ratio;//arbaro does this which makes more sense imo. Paper does 0.2 + 0.8 * ratio; 
    /*spherical*/           if(shape == 1) return 0.2 + 0.8 * sin(PI * ratio);
    /*hemispherical*/       if(shape == 2) return 0.2 + 0.8 * sin(0.5 * PI * ratio);
    /*cylindrical*/         if(shape == 3) return 1.0;
    /*tapered cylindrical*/ if(shape == 4) return 0.5 + 0.5 * ratio;
    /*flame*/               if(shape == 5) return (ratio <= 0.7) ? (ratio / 0.7) : ((1.0 - ratio) / 0.3);
    /*inverse conical*/     if(shape == 6) return 1.0 - 0.8 * ratio;
    /*tend flame*/          if(shape == 7) return 0.5 + 0.5 * ((ratio <= 0.7) ? (ratio / 0.7) : ((1.0 - ratio) / 0.3));
    return 0.0;
}

float SmoothAbs(float x, float s = 0.00001){
    return sqrt(x*x + s);
}

float Flare(in const SegmentInfo si, in const TreeParameters params, in const float z){
    if(si.level != 0) return 1;
    float y = max(0, 1 - 8 * z);
    return params.Flare * (pow(100, y) - 1) / 100. + 1;
}

float GetTaperedRadius(in const SegmentInfo si, in const TreeParameters params, in  float z){
    if(z > 0.9999) return 0;
    
    float taper = params.nTaper[si.level];    
    if(taper < 1){
        taper = 2 - taper;
    }
    taper -= 1;
    
    bool isPalm = taper >= 1;
    
    float unit_taper = max(0, 1 - taper);
    float tr = si.radius * (1 - unit_taper * z);
    
    float z2 = (1-z) * si.length;
    float z3 = !isPalm ? z2 : SmoothAbs(z2 - 2 * tr * round(z2 / (2 * tr)));
              
    float a = tr * tr;
    float b = (z3 - tr) * (z3 - tr);        
    float r = sqrt(max(0, a - b));
    
    float depth = 0;
    
    if(isPalm && (z2 >= tr)){ // tip of palm
        depth = 2 - taper;
    }    
    if(!isPalm && !(z3 < tr)){
        depth = 1;
    }
    
    float taperedRadius = lerp(r, tr, depth);
   
    return taperedRadius * Flare(si, params, z);
}

float GetTaperedRadius_(in const SegmentInfo si, in const TreeParameters params, in const float z){
    static const float h = 0.00001;
    return (GetTaperedRadius(si, params, z) - GetTaperedRadius(si, params, z - h)) / (h);
}

float GetLobeFactor(in const SegmentInfo si, in const TreeParameters params, in const float theta, in const float t){
    return lerp(1.0 + params.LobeDepth * sin(params.Lobes * theta), 1, saturate(t));
}

float GetLobeFactor_(in const SegmentInfo si, in const TreeParameters params, in const float theta, in const float t){
    return params.LobeDepth * params.Lobes * (1 - t) * cos(params.Lobes * theta);
}

float MapToRange(float v, float min, float max){
    return min + v * (max - min);
}

float4 CatmullRomSpline(float t){
    const float t2 = t * t;
    const float t3 = t2 * t;
    return float4(
        +2 * t3 -3 * t2 +1,
        +1 * t3 -2 * t2 +t,
        -2 * t3 +3 * t2,
        +1 * t3 -1 * t2
    );
}

float4 CatmullRomSpline_(float t){
    const float t2 = t * t;
    return float4(
        +6 * t2 -6 * t,
        +3 * t2 -4 * t +1,
        -6 * t2 +6 * t,
        +3 * t2 -2 * t
    );
}

float3 StemSpline(
    float3 fromPos,
    float3 fromZ,
    float3 toPos,
    float3 toZ,
    float t
){  
    // required for weighting the tangents when evaluating the CR spline
    const float l = distance(fromPos, toPos);
    return mul(CatmullRomSpline(t), float4x3(fromPos, fromZ * l, toPos, toZ * l));
}

float3 StemSpline_(
    float3 fromPos,
    float3 fromZ,
    float3 toPos,
    float3 toZ,
    float t
){
    // required for weighting the tangents when evaluating the CR spline
    const float l = distance(fromPos, toPos);
    return mul(CatmullRomSpline_(t), float4x3(fromPos, fromZ * l, toPos, toZ * l));
}

float fakeAOfromDistance(float distance){
    return pow(clamp(1-distance * 0.015, 0.8, 1), 3);
}

float GetNoisedLeafSeason(in const uint seed){
    float scale = 0.2 + 0.1 * GetSeason();
    return (GetSeason() + scale * random::Random(seed, 0xDEAD));
}

float GetSeasonLeafScale(in const uint seed, in const bool isBlossom){
    float season = GetNoisedLeafSeason(seed);
    float growth = saturate(season);
    if (!isBlossom) {
        growth = pow(growth, 2);
    }
    bool fall = season < (isBlossom? 1.75 : 3.75);
    return fall ? growth : 0;
}

float3 GetSeasonLeafColor(in const LeafParameters params, in float season, in bool isBlossom){
    const float3 summer           = 1.1 * params.Color.rgb;

    if(params.IsNeedle || isBlossom) return summer;
    season = clamp(season, 0, 4.);
    const float3        spring    = 1.1 * summer;
    static const float3 earlyFall = 0.5 * float3(.99, .74, .17);
    static const float3 lateFall  = 0.5 * float3(.55, .10, .03);
    
    static const float4 config[] = {
        float4(spring, -1e9),
        float4(spring, 1),
        float4(summer, 2.3),
        float4(earlyFall, 3),
        float4(lateFall, 3.5),
        float4(lateFall, 1e9),
    };
    static const int length = sizeof(config)/sizeof(float4);
        
    for(int i = 1; i < length; ++i){
        if(season < config[i].w){
            float t = (season - config[i-1].w) / (config[i].w - config[i-1].w);
            return lerp(config[i-1].rgb, config[i].rgb, t);
        }
    }
    return config[length-1].rgb;
}

float GetGeneralSeasonFruitProgress(float season){
    if(season < 1.5) return 0;
    if(season > 3.25) return 0;
    return min(season - 1.5, 1);
}

float GetSeasonFruitProgress(in const uint seed){
    float season = GetSeason() + 0.025 * random::SignedRandom(seed, 0xBEAF);
    return GetGeneralSeasonFruitProgress(season);
}

float GetFruitScale(in const float progress){
    return pow(progress, .25);
}

bool IsLeafBlossom(in const TreeParameters treeParameters, in const uint stemChildIndex) {
    const float ratio = treeParameters.Blossom.Count / float(treeParameters.Blossom.Count + treeParameters.Leaf.Count);
    return random::Random(stemChildIndex) < ratio;
}