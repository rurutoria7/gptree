#pragma once

#include "Shading.h"
#include "Records.h"
#include "TreeModel.h"
#include "Camera.h"

static const int maxDrawFruitGroupsPerDispatch = 256;

struct DrawFruitRecordBundle {
    uint dispatchGrid : SV_DispatchGrid;
    DrawLeafRecord fruits[maxDrawFruitGroupsPerDispatch];
};

[Shader("node")]
[NodeLaunch("coalescing")]
[NodeId("CoalesceDrawLeaves", 2)]
[NumThreads(maxDrawFruitGroupsPerDispatch, 1, 1)]
void CoalesceDrawFruits(
    uint gtid : SV_GroupThreadID,

    [MaxRecords(maxDrawFruitGroupsPerDispatch)]
    GroupNodeInputRecords<DrawLeafRecord> irs,

    [MaxRecords(1)]
    [NodeId("DrawFruitBundle")]
    NodeOutput<DrawFruitRecordBundle> output
){
    GroupNodeOutputRecords<DrawFruitRecordBundle> outputRecord = output.GetGroupNodeOutputRecords(1);

    const TreeParameters treeParams = GetTreeParameters();

    outputRecord.Get().dispatchGrid = irs.Count();

    if (gtid < irs.Count()) {
        outputRecord.Get().fruits[gtid] = irs.Get(gtid);
    }

    outputRecord.OutputComplete();
}

struct FruitVertex{
    float4 clipSpacePosition  : SV_POSITION;
    float4 rotation : NORMAL0;
    float3 uvt : NORMAL1;
};

struct FruitPrimitive {
    uint seed : BLENDINDICES0;
};


static const int maxNumVerticesPerFruitGroup = 130;
static const int maxNumTrianglesPerFruitGroup = 256;
static const float3 positions[maxNumVerticesPerFruitGroup] = {
    float3(0.0000, 0.4330, 0.7500),     float3(0.0000, 0.4924, 0.5868),     float3(0.0000, 0.4924, 0.4132),     float3(0.0000, 0.4330, 0.2500),     float3(0.0000, 0.1710, 0.0302),     float3(0.0654, 0.1580, 0.9698),     float3(0.1230, 0.2969, 0.8830),     float3(0.1657, 0.4001, 0.7500),     float3(0.1884, 0.4549, 0.5868),     float3(0.1884, 0.4549, 0.4132),     float3(0.1657, 0.4001, 0.2500),     float3(0.1230, 0.2969, 0.1170),     float3(0.0654, 0.1580, 0.0302),     float3(0.1209, 0.1209, 0.9698),     float3(0.2273, 0.2273, 0.8830),     float3(0.3062, 0.3062, 0.7500),     float3(0.3482, 0.3482, 0.5868),     float3(0.3482, 0.3482, 0.4132),     float3(0.3062, 0.3062, 0.2500),     float3(0.2273, 0.2273, 0.1170),     float3(0.1209, 0.1209, 0.0302),     float3(0.1580, 0.0654, 0.9698),     float3(0.2969, 0.1230, 0.8830),     float3(0.4001, 0.1657, 0.7500),     float3(0.4549, 0.1884, 0.5868),     float3(0.4549, 0.1884, 0.4132),     float3(0.4001, 0.1657, 0.2500),     float3(0.2969, 0.1230, 0.1170),     float3(0.1580, 0.0654, 0.0302),     float3(0.1710, -0.0000, 0.9698),     float3(0.3214, -0.0000, 0.8830),     float3(0.4330, -0.0000, 0.7500),     float3(0.4924, -0.0000, 0.5868),     float3(0.4924, -0.0000, 0.4132),     float3(0.4330, -0.0000, 0.2500),     float3(0.3214, -0.0000, 0.1170),     float3(0.1710, -0.0000, 0.0302),     float3(0.1580, -0.0654, 0.9698),     float3(0.2969, -0.1230, 0.8830),     float3(0.4001, -0.1657, 0.7500),     float3(0.4549, -0.1884, 0.5868),     float3(0.4549, -0.1884, 0.4132),     float3(0.4001, -0.1657, 0.2500),     float3(0.2969, -0.1230, 0.1170),     float3(0.1580, -0.0654, 0.0302),     float3(0.1209, -0.1209, 0.9698),     float3(0.2273, -0.2273, 0.8830),     float3(0.3062, -0.3062, 0.7500),     float3(0.3482, -0.3482, 0.5868),     float3(0.3482, -0.3482, 0.4132),     float3(0.3062, -0.3062, 0.2500),     float3(0.2273, -0.2273, 0.1170),     float3(0.1209, -0.1209, 0.0302),     float3(0.0654, -0.1580, 0.9698),     float3(0.1230, -0.2969, 0.8830),     float3(0.1657, -0.4001, 0.7500),     float3(0.1884, -0.4549, 0.5868),     float3(0.1884, -0.4549, 0.4132),     float3(0.1657, -0.4001, 0.2500),     float3(0.1230, -0.2969, 0.1170),     float3(0.0654, -0.1580, 0.0302),     float3(-0.0000, -0.1710, 0.9698),     float3(-0.0000, -0.3214, 0.8830),     float3(-0.0000, -0.4330, 0.7500),     float3(-0.0000, -0.4924, 0.5868),     float3(-0.0000, -0.4924, 0.4132),     float3(-0.0000, -0.4330, 0.2500),     float3(-0.0000, -0.3214, 0.1170),     float3(-0.0000, -0.1710, 0.0302),     float3(0.0000, 0.0000, 1.0000),     float3(-0.0654, -0.1580, 0.9698),     float3(-0.1230, -0.2969, 0.8830),     float3(-0.1657, -0.4001, 0.7500),     float3(-0.1884, -0.4549, 0.5868),     float3(-0.1884, -0.4549, 0.4132),     float3(-0.1657, -0.4001, 0.2500),     float3(-0.1230, -0.2969, 0.1170),     float3(-0.0654, -0.1580, 0.0302),     float3(-0.1209, -0.1209, 0.9698),     float3(-0.2273, -0.2273, 0.8830),     float3(-0.3062, -0.3062, 0.7500),     float3(-0.3482, -0.3482, 0.5868),     float3(-0.3482, -0.3482, 0.4132),     float3(-0.3062, -0.3062, 0.2500),     float3(-0.2273, -0.2273, 0.1170),     float3(-0.1209, -0.1209, 0.0302),     float3(0.0000, 0.0000, 0.0000),     float3(-0.1580, -0.0654, 0.9698),     float3(-0.2969, -0.1230, 0.8830),     float3(-0.4001, -0.1657, 0.7500),     float3(-0.4549, -0.1884, 0.5868),     float3(-0.4549, -0.1884, 0.4132),     float3(-0.4001, -0.1657, 0.2500),     float3(-0.2969, -0.1230, 0.1170),     float3(-0.1580, -0.0654, 0.0302),     float3(-0.1710, 0.0000, 0.9698),     float3(-0.3214, 0.0000, 0.8830),     float3(-0.4330, 0.0000, 0.7500),     float3(-0.4924, 0.0000, 0.5868),     float3(-0.4924, 0.0000, 0.4132),     float3(-0.4330, 0.0000, 0.2500),     float3(-0.3214, 0.0000, 0.1170),     float3(-0.1710, 0.0000, 0.0302),     float3(-0.1580, 0.0654, 0.9698),     float3(-0.2969, 0.1230, 0.8830),     float3(-0.4001, 0.1657, 0.7500),     float3(-0.4549, 0.1884, 0.5868),     float3(-0.4549, 0.1884, 0.4132),     float3(-0.4001, 0.1657, 0.2500),     float3(-0.2969, 0.1230, 0.1170),     float3(-0.1580, 0.0654, 0.0302),     float3(-0.1209, 0.1209, 0.9698),     float3(-0.2273, 0.2273, 0.8830),     float3(-0.3062, 0.3062, 0.7500),     float3(-0.3482, 0.3482, 0.5868),     float3(-0.3482, 0.3482, 0.4132),     float3(-0.3062, 0.3062, 0.2500),     float3(-0.2273, 0.2273, 0.1170),     float3(-0.1209, 0.1209, 0.0302),     float3(-0.0654, 0.1580, 0.9698),     float3(-0.1230, 0.2969, 0.8830),     float3(-0.1657, 0.4001, 0.7500),     float3(-0.1884, 0.4549, 0.5868),     float3(-0.1884, 0.4549, 0.4132),     float3(-0.1657, 0.4001, 0.2500),     float3(-0.1230, 0.2969, 0.1170),     float3(-0.0654, 0.1580, 0.0302),     float3(0.0000, 0.1710, 0.9698),     float3(0.0000, 0.3214, 0.8830),     float3(0.0000, 0.3214, 0.1170), };
static const uint3 triangles[maxNumTrianglesPerFruitGroup] = {
    uint3(2, 8, 9),     uint3(128, 5, 6),     uint3(86, 4, 12),     uint3(3, 9, 10),     uint3(0, 6, 7),     uint3(129, 10, 11),     uint3(1, 7, 8),     uint3(127, 69, 5),     uint3(4, 11, 12),     uint3(7, 14, 15),     uint3(11, 18, 19),     uint3(8, 15, 16),     uint3(5, 69, 13),     uint3(12, 19, 20),     uint3(9, 16, 17),     uint3(6, 13, 14),     uint3(86, 12, 20),     uint3(10, 17, 18),     uint3(13, 69, 21),     uint3(20, 27, 28),     uint3(17, 24, 25),     uint3(14, 21, 22),     uint3(86, 20, 28),     uint3(18, 25, 26),     uint3(15, 22, 23),     uint3(19, 26, 27),     uint3(16, 23, 24),     uint3(86, 28, 36),     uint3(26, 33, 34),     uint3(23, 30, 31),     uint3(27, 34, 35),     uint3(24, 31, 32),     uint3(21, 69, 29),     uint3(28, 35, 36),     uint3(25, 32, 33),     uint3(22, 29, 30),     uint3(35, 42, 43),     uint3(32, 39, 40),     uint3(29, 69, 37),     uint3(36, 43, 44),     uint3(33, 40, 41),     uint3(30, 37, 38),     uint3(86, 36, 44),     uint3(34, 41, 42),     uint3(31, 38, 39),     uint3(41, 48, 49),     uint3(38, 45, 46),     uint3(86, 44, 52),     uint3(42, 49, 50),     uint3(39, 46, 47),     uint3(43, 50, 51),     uint3(40, 47, 48),     uint3(37, 69, 45),     uint3(44, 51, 52),     uint3(47, 54, 55),     uint3(51, 58, 59),     uint3(48, 55, 56),     uint3(45, 69, 53),     uint3(52, 59, 60),     uint3(49, 56, 57),     uint3(46, 53, 54),     uint3(86, 52, 60),     uint3(50, 57, 58),     uint3(53, 69, 61),     uint3(60, 67, 68),     uint3(57, 64, 65),     uint3(54, 61, 62),     uint3(86, 60, 68),     uint3(58, 65, 66),     uint3(55, 62, 63),     uint3(59, 66, 67),     uint3(56, 63, 64),     uint3(66, 74, 75),     uint3(63, 71, 72),     uint3(67, 75, 76),     uint3(64, 72, 73),     uint3(61, 69, 70),     uint3(68, 76, 77),     uint3(65, 73, 74),     uint3(62, 70, 71),     uint3(86, 68, 77),     uint3(73, 80, 81),     uint3(70, 69, 78),     uint3(77, 84, 85),     uint3(74, 81, 82),     uint3(71, 78, 79),     uint3(86, 77, 85),     uint3(75, 82, 83),     uint3(72, 79, 80),     uint3(76, 83, 84),     uint3(79, 87, 88),     uint3(86, 85, 94),     uint3(83, 91, 92),     uint3(80, 88, 89),     uint3(84, 92, 93),     uint3(81, 89, 90),     uint3(78, 69, 87),     uint3(85, 93, 94),     uint3(82, 90, 91),     uint3(93, 100, 101),     uint3(90, 97, 98),     uint3(87, 69, 95),     uint3(94, 101, 102),     uint3(91, 98, 99),     uint3(88, 95, 96),     uint3(86, 94, 102),     uint3(92, 99, 100),     uint3(89, 96, 97),     uint3(102, 109, 110),     uint3(99, 106, 107),     uint3(96, 103, 104),     uint3(86, 102, 110),     uint3(100, 107, 108),     uint3(97, 104, 105),     uint3(101, 108, 109),     uint3(98, 105, 106),     uint3(95, 69, 103),     uint3(108, 115, 116),     uint3(105, 112, 113),     uint3(109, 116, 117),     uint3(106, 113, 114),     uint3(103, 69, 111),     uint3(110, 117, 118),     uint3(107, 114, 115),     uint3(104, 111, 112),     uint3(86, 110, 118),     uint3(114, 121, 122),     uint3(111, 69, 119),     uint3(118, 125, 126),     uint3(115, 122, 123),     uint3(112, 119, 120),     uint3(86, 118, 126),     uint3(116, 123, 124),     uint3(113, 120, 121),     uint3(117, 124, 125),     uint3(120, 127, 128),     uint3(86, 126, 4),     uint3(124, 2, 3),     uint3(121, 128, 0),     uint3(125, 3, 129),     uint3(122, 0, 1),     uint3(119, 69, 127),     uint3(126, 129, 4),     uint3(123, 1, 2),     uint3(2, 1, 8),     uint3(128, 127, 5),     uint3(3, 2, 9),     uint3(0, 128, 6),     uint3(129, 3, 10),     uint3(1, 0, 7),     uint3(4, 129, 11),     uint3(7, 6, 14),     uint3(11, 10, 18),     uint3(8, 7, 15),     uint3(12, 11, 19),     uint3(9, 8, 16),     uint3(6, 5, 13),     uint3(10, 9, 17),     uint3(20, 19, 27),     uint3(17, 16, 24),     uint3(14, 13, 21),     uint3(18, 17, 25),     uint3(15, 14, 22),     uint3(19, 18, 26),     uint3(16, 15, 23),     uint3(26, 25, 33),     uint3(23, 22, 30),     uint3(27, 26, 34),     uint3(24, 23, 31),     uint3(28, 27, 35),     uint3(25, 24, 32),     uint3(22, 21, 29),     uint3(35, 34, 42),     uint3(32, 31, 39),     uint3(36, 35, 43),     uint3(33, 32, 40),     uint3(30, 29, 37),     uint3(34, 33, 41),     uint3(31, 30, 38),     uint3(41, 40, 48),     uint3(38, 37, 45),     uint3(42, 41, 49),     uint3(39, 38, 46),     uint3(43, 42, 50),     uint3(40, 39, 47),     uint3(44, 43, 51),     uint3(47, 46, 54),     uint3(51, 50, 58),     uint3(48, 47, 55),     uint3(52, 51, 59),     uint3(49, 48, 56),     uint3(46, 45, 53),     uint3(50, 49, 57),     uint3(60, 59, 67),     uint3(57, 56, 64),     uint3(54, 53, 61),     uint3(58, 57, 65),     uint3(55, 54, 62),     uint3(59, 58, 66),     uint3(56, 55, 63),     uint3(66, 65, 74),     uint3(63, 62, 71),     uint3(67, 66, 75),     uint3(64, 63, 72),     uint3(68, 67, 76),     uint3(65, 64, 73),     uint3(62, 61, 70),     uint3(73, 72, 80),     uint3(77, 76, 84),     uint3(74, 73, 81),     uint3(71, 70, 78),     uint3(75, 74, 82),     uint3(72, 71, 79),     uint3(76, 75, 83),     uint3(79, 78, 87),     uint3(83, 82, 91),     uint3(80, 79, 88),     uint3(84, 83, 92),     uint3(81, 80, 89),     uint3(85, 84, 93),     uint3(82, 81, 90),     uint3(93, 92, 100),     uint3(90, 89, 97),     uint3(94, 93, 101),     uint3(91, 90, 98),     uint3(88, 87, 95),     uint3(92, 91, 99),     uint3(89, 88, 96),     uint3(102, 101, 109),     uint3(99, 98, 106),     uint3(96, 95, 103),     uint3(100, 99, 107),     uint3(97, 96, 104),     uint3(101, 100, 108),     uint3(98, 97, 105),     uint3(108, 107, 115),     uint3(105, 104, 112),     uint3(109, 108, 116),     uint3(106, 105, 113),     uint3(110, 109, 117),     uint3(107, 106, 114),     uint3(104, 103, 111),     uint3(114, 113, 121),     uint3(118, 117, 125),     uint3(115, 114, 122),     uint3(112, 111, 119),     uint3(116, 115, 123),     uint3(113, 112, 120),     uint3(117, 116, 124),     uint3(120, 119, 127),     uint3(124, 123, 2),     uint3(121, 120, 128),     uint3(125, 124, 3),     uint3(122, 121, 0),     uint3(126, 125, 129),     uint3(123, 122, 1), };


float3 QuadraticBezier(in const float t){
    float u = 1 - t;
    float3 w;
    w.x = u * u;
    w.y = 2 * t * u;
    w.z = t * t;
    return w;
}

float3 QuadraticBezier_(in const float t) {
    float u = 1 - t;
    float3 w;
    w.x = -2 * u;
    w.y = 2 * (u - t);
    w.z = 2 * t;
    return w;
}

float4 CubicBezier(in const float t){
    float u = 1 - t;
    float tt = t * t;
    float uu = u * u;
    float4 w;
    w.x = uu*u;
    w.y = 3 * t * uu;
    w.z = 3 * tt * u;
    w.w = tt*t;
    return w;
}

float4 CubicBezier_(in const float t) {
    float u = 1 - t;
    float tt = t * t;
    float uu = u * u;
    float4 w;
    w.x = -3 * uu;
    w.y = 3 * (uu - 2 * t * u);
    w.z = 3 * (2 * t * u - tt);
    w.w = 3 * tt;
    return w;
}

template<typename T>
T SafeNormalize(in const T v){
    float l = length(v);
    if(l > 0) return v / l;
    return v;
}

[Shader("node")]
[NodeLaunch("mesh")]
[NodeId("DrawFruitBundle")]
[NodeMaxDispatchGrid(maxDrawFruitGroupsPerDispatch, 1, 1)]
[NumThreads(128, 1, 1)]
[OutputTopology("triangle")]
void FruitMeshShader(
    uint gid : SV_GroupID,
    uint gtid : SV_GroupThreadID,
    DispatchNodeInputRecord<DrawFruitRecordBundle> ir,
    out vertices FruitVertex verts[maxNumVerticesPerFruitGroup],
    out indices uint3 tris[maxNumTrianglesPerFruitGroup],
    out primitives FruitPrimitive prims[maxNumTrianglesPerFruitGroup]
){
    SetMeshOutputCounts(maxNumVerticesPerFruitGroup, maxNumTrianglesPerFruitGroup);

    const FruitParameters params = GetTreeParameters().Fruit;

    const DrawLeafRecord record = ir.Get().fruits[gid];

    static const int vertexLoops = (maxNumVerticesPerFruitGroup + 127) / 128;
    for(int i = 0; i < vertexLoops; ++i){
        int vertId = 128 * i + gtid;
        if(vertId < maxNumVerticesPerFruitGroup){
            FruitVertex vertex;


            vertex.rotation = record.trafo.GetRot();
            vertex.uvt = positions[vertId];

            float t = saturate(positions[vertId].z);

            float2 s = mul(CubicBezier(t), float4x2(float2(0, 0), params.Shape.xy, params.Shape.zw, float2(0, 1)));
            float3 modelSpacePosition;
            modelSpacePosition.xy = s.x * SafeNormalize(positions[vertId].xy);
            modelSpacePosition.z = s.y;

            modelSpacePosition *= record.scale;

            float3 rotatedPosition = qTransform(record.trafo.GetRot(), modelSpacePosition);

            float3 worldSpacePosition = record.trafo.GetPos() + rotatedPosition;

            vertex.clipSpacePosition = mul(GetViewProjectionMatrix(), float4(worldSpacePosition, 1));

            verts[vertId] = vertex;
        }
    }

    static const int triangleLoops = (maxNumTrianglesPerFruitGroup + 127) / 128;
    for(int i = 0; i < triangleLoops; ++i){
        int triId = 128 * i + gtid;
        if(gtid < maxNumTrianglesPerFruitGroup){
            tris[triId] = triangles[triId];
            prims[triId].seed = record.seed;
        }
    }
}

float4 FruitPixelShader(
    const in FruitVertex vertex,
    const in FruitPrimitive primitive,
    const in float3 barycentrics : SV_Barycentrics,
    const in bool isFrontFace : SV_IsFrontFace
) : SV_Target0
{
    float progress = GetSeasonFruitProgress(primitive.seed) + (1 - vertex.uvt.z) * .5;

    SurfaceData surface;
    surface.metallic     = 0;
    surface.roughness    = 1 - 0.38 * progress;
    surface.occlusion    = 1;
    surface.translucency = 0;
    surface.baseColor.a  = 1;

    const FruitParameters params = GetTreeParameters().Fruit;

    float2 s = mul(CubicBezier(vertex.uvt.z), float4x2(float2(0, 0), params.Shape.xy, params.Shape.zw, float2(0, 1)));

    float3 modelSpacePosition;
    modelSpacePosition.xy = s.x * SafeNormalize(vertex.uvt.xy);
    modelSpacePosition.z = s.y;

    float3 rotatedPosition = qTransform(vertex.rotation, modelSpacePosition);
    surface.normal = Normal(rotatedPosition);

    surface.baseColor.rgb = lerp(GetTreeParameters().Leaf.Color.rgb, params.Color.rgb, 0.3 + progress*progress);

    return ShadeSurface(surface);
 }
