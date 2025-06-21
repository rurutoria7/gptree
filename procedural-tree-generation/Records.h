#pragma once

#include "Quaternion.h"
#include "TreeParameters.h"

// ========================= Constants ====================
// List of constants for records (e.g. coalescing factors)

#define LEVEL_BITS             2
#define SEGMENT_Z_BITS         15
#define TREE_MAX_LOBE_COUNT    5

static const int maxNumTriangleRingsPerSegment = 128;
static const int maxNumVerticesPerGroup  = 128;
static const int maxNumTrianglesPerGroup = 128;

// ========================== Records =====================

struct SkyboxRecord {
    uint test;
};

struct TreeTransform {
    float3 pos;
    float4 rot;

    // no-op to match interface of compressed tree transforms
    TreeTransform Decompress()
    {
        TreeTransform result;

        result.pos = pos;
        result.rot = rot;

        return result;
    }

    float3 GetPos() { return pos; }
    void SetPos(float3 ipos) { pos = ipos; }
    float4 GetRot() { return rot; }
    void SetRot(float4 irot) { rot = irot; }
};

struct TreeTransformCompressedq32 {
    float4 trafo;
        
    TreeTransform Decompress() {
        TreeTransform result;
            
        result.pos = GetPos();
        result.rot = GetRot();
            
        return result;
    }
        
    float3 GetPos(){ return trafo.xyz; }
    void SetPos(float3 ipos){ trafo.xyz = ipos; }
    float4 GetRot(){ return qDecompress32(asuint(trafo.w)); }
    void SetRot(float4 irot){ trafo.w = asfloat(qCompress32(irot)); }
};

struct TreeTransformCompressedq64 {
    float3 pos;
    uint64_t rot;
        
    TreeTransform Decompress() {
        TreeTransform result;
            
        result.pos = GetPos();
        result.rot = GetRot();
            
        return result;
    }
        
    float3 GetPos(){ return pos; }
    void SetPos(float3 ipos){ pos = ipos; }
    float4 GetRot(){ return qDecompress64(rot); }
    void SetRot(float4 irot){ rot = qCompress64(irot); }
};

struct StemTubeCage {
    TreeTransform from;
    TreeTransform to;
};

struct StemTubeCageCompressed {
    TreeTransformCompressedq32 from;
    TreeTransformCompressedq32 to;
    
    StemTubeCage Decompress(){
        StemTubeCage cage;
        cage.from.pos = from.GetPos();
        cage.from.rot = from.GetRot();
        cage.to.pos   = to.GetPos();
        cage.to.rot   = to.GetRot();
        return cage;
    }
};

struct SegmentInfo {
    uint level        : LEVEL_BITS;
    uint fromZ        : SEGMENT_Z_BITS;
    uint toZ          : SEGMENT_Z_BITS;
    float length;
    float radius;
    
    static uint EncodeZ(in float z) {
        const float zClamped        = clamp(z, 0, 1);
        const uint  maxEncodedValue = (1 << SEGMENT_Z_BITS) - 1;
        const uint  encodedValue    = round(zClamped * maxEncodedValue);
    
        return clamp(encodedValue, 0, maxEncodedValue);
    }
    
    static float DecodeZ(in uint encodedZ) {
        const uint  maxEncodedValue = (1 << SEGMENT_Z_BITS) - 1;
        const float decodedValue    = float(encodedZ) / float(maxEncodedValue);
    
        return clamp(decodedValue, 0, 1);
    }
    
    void SetFromZ(in float z) {
        fromZ = EncodeZ(z);
    }
    
    float GetFromZ() {
        return DecodeZ(fromZ);
    }
    
    void SetToZ(in float z) {
        toZ = EncodeZ(z);
    }
    
    float GetToZ() {
        return DecodeZ(toZ);
    }
};

typedef uint3 PackedSegmentInfo;

struct DrawSegmentRecord {
    StemTubeCageCompressed cage;
    SegmentInfo  si;
    float aoDistance;
    float fromPoints;
    float toPoints;
    float vPoints;
    int faceRingsPerGroup;
    float fromOpeningAngle;
    float toOpeningAngle;
    uint dispatchGrid : SV_DispatchGrid;
};

struct DrawLeafRecord {
    TreeTransformCompressedq32 trafo;
    
    uint  seed;
    float scale;
    float aoDistance;
};

struct GenerateTreeRecord {
    TreeTransformCompressedq64 trafo;
    uint seed;
    uint children;
    float scale;
    float length;
    float radius;
    float aoDistance;
};

// =========================== Utils ======================

GenerateTreeRecord CreateTreeRecord(in const float3 position, in const float4 rotation, in const uint seed)
{
    const TreeParameters params = GetTreeParameters();

    GenerateTreeRecord record;

    record.trafo.SetPos(position);
    record.trafo.SetRot(rotation);

    record.seed = seed;
    record.aoDistance = 0;

    record.scale = params.Scale + .5 * params.ScaleV * random::SignedRandom(record.seed, 2413);
    record.length = record.scale * (params.nLength[0] + params.nLengthV[0] * random::SignedRandom(record.seed, 123));
    record.radius = record.length * params.Ratio;
    record.children = (params.Levels == 1) ? (params.Leaf.Count + params.Blossom.Count) : params.nBranches[1];

    return record;
}

PackedSegmentInfo PackSegmentInfo(in const SegmentInfo si)
{
    // This packs the bit-field, not sure about the floats, so they are handled manually
    PackedSegmentInfo sip = (uint3)si;
    sip.y                 = asuint(si.length);
    sip.z                 = asuint(si.radius);
    return sip;
}

SegmentInfo UnpackSegmentInfo(in const PackedSegmentInfo sip)
{
    // this initializes the bit-field, so we only need to handle the two floats
    SegmentInfo si = (SegmentInfo)sip.x;
    si.length      = asfloat(sip.y);
    si.radius      = asfloat(sip.z);
    return si;
}

float RoundUpMultiple2(float x)
{
    return ceil(x / 2) * 2;
}

template <typename T>
T MapRange(in const T value, in const T sMin, in const T sMax, in const T dMin, in const T dMax)
{
    const T value01 = clamp((value - sMin) / (sMax - sMin), 0, 1);
        
    return dMin + (value01 * (dMax - dMin));
}

bool IsBitSet(in uint data, in int bitIndex)
{
    return data & (1u << bitIndex);
}

float Sign(in const float f){
    return asfloat((asuint(f) & asuint(-0.)) | asuint(1.));
}

int BitSign(in uint data, in int bitIndex)
{
    return IsBitSet(data, bitIndex) ? 1 : -1;
}