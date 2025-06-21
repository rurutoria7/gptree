#pragma once

#include "Records.h"
#include "Camera.h"
#include "Config.h"
#include "TreeModel.h"
#include "Shading.h"

namespace splineSegment {

    static const float bumpLevelBias = 0;

    float r(
        in const SegmentInfo si,
        in const TreeParameters params,
        in const float theta,
        in const float z,
        in const float t
    ){
        float radius = GetTaperedRadius(si, params, z);

        // add lobe to radius
        if(si.level == 0 && si.fromZ == 0){
            radius *= GetLobeFactor(si, params, theta, t);
        }

        return radius;
    }

    int2 Wrap(int2 v, int xWrap){
        v.x = (v.x + 256 * xWrap) % xWrap;
        return v;
    }

    float WrappingXPerlinNoise(in float2 position, int xWrap) {
        const int2   grid = floor(position);
        const float2 uv  = frac(position);
        const int a = (grid.x + 0) % xWrap;
        const int b = (grid.x + 1) % xWrap;

        const float d00 = dot(random::PerlinNoiseDir2D(int2(a, grid.y + 0)), uv - float2(0, 0));
        const float d01 = dot(random::PerlinNoiseDir2D(int2(a, grid.y + 1)), uv - float2(0, 1));
        const float d10 = dot(random::PerlinNoiseDir2D(int2(b, grid.y + 0)), uv - float2(1, 0));
        const float d11 = dot(random::PerlinNoiseDir2D(int2(b, grid.y + 1)), uv - float2(1, 1));

        const float2 interpolationWeights = uv * uv * uv * (uv * (uv * 6 - 15) + 10);
        const float d0 = lerp(d00, d01, interpolationWeights.y);
        const float d1 = lerp(d10, d11, interpolationWeights.y);
        return lerp(d0, d1, interpolationWeights.x);
    }

    float CloudNoiseWrapX(float2 pos, int xWrap, int levels=8){
        int n = clamp(floor(levels), 1, 8);
        float noise = 0;
        for(int i = 0; i < n; ++i){
            float s = pow(2, i);
            noise += WrappingXPerlinNoise(pos * s, xWrap) / s;
            pos += 20;
            xWrap *= 2;
        }
        return noise;
    }

    float hash1( float n ) { return frac(sin(n)*43758.5453); }
    float2 hash2( float2 p ) { return frac(sin(float2(dot(p,float2(127.1,311.7)),dot(p,float2(269.5,183.3))))*43758.5453);}

    float Voronoi( in float2 x )
    {
        int2 p = floor( x );
        float2  f = frac( x );

        float res = 8.0;
        for( int j=-1; j<=1; j++ )
        for( int i=-1; i<=1; i++ )
        {
            int2 b = int2( i, j );
            float2  r = float2( b ) - f + hash2( p + b );
            float d = dot( r, r );

            res = min( res, d );
        }
        return sqrt( res );
    }


    float VoronoiDistanceWrapX(in float2 x, int xWrap)
    {
        int2 p = int2(floor(x));
        p = Wrap(p, xWrap);

        float2 f = frac(x);

        int2 mb;
        float2 mr;

        float res = 8.0;
        for( int j=-1; j<=1; j++ )
        for( int i=-1; i<=1; i++ )
        {
            int2 b = int2(i, j);
            int2 h = Wrap(p + b, xWrap);
            float2 r = float2(b) + hash2(h) - f;
            float d = dot(r,r);

            if(d < res)
            {
                res = d;
                mr = r;
                mb = b;
            }
        }

        res = 8.0;
        for( int j=-2; j<=2; j++ )
        for( int i=-2; i<=2; i++ )
        {
            int2 b = mb + int2(i, j);
            int2 h = Wrap(p + b, xWrap);
            float2  r = float2(b) + hash2(h) - f;
            float d = dot(0.5*(mr+r), normalize(r-mr));

            res = min( res, d );
        }

        return res;
    }


    float SnormToUnorm(float x){
        return (x + 1) * .5;
    }
    float UnormToSnorm(float x){
        return (2 * x) - 1;
    }

    float Unormalize(float x, float m, float M){
        return (x - m) / (M - m);
    }

    float LerpIn(float nearValue, float switchStart, float switchEnd, float distance){
        float t = saturate(Unormalize(distance, switchStart, switchEnd));
        return nearValue * t;
    }

    float SmoothTessellation(int p, float xf, int x){
        float step = (xf - x + 2) * .5 / (x - 1);
        float t = lerp(step, 1. - step, float(p-1) / (x - 3));
        t = clamp(t, 0, 1);
        return t;
    }

    struct StemVertex {
        float4 clipSpacePosition : SV_POSITION;
        float4 openingAngle_u_v_z : NORMAL1;
        float  ao : BLENDWEIGHTS0;

        nointerpolation float4 fromTrafo : POSITION3;
        nointerpolation float4 toTrafo   : POSITION6;
    };

    struct StemPrimitive {
        uint3 sip : BLENDINDICES0;
    };

}

[Shader("node")]
[NodeLaunch("mesh")]
[NodeId("DrawSegment", 0)]
[NodeMaxDispatchGrid(maxNumTriangleRingsPerSegment, 1, 1)]
[NumThreads(128, 1, 1)]
[OutputTopology("triangle")]
void StemMeshShader(
    uint gtid : SV_GroupThreadID,
    uint groupOfSegment : SV_GroupID,
    DispatchNodeInputRecord<DrawSegmentRecord> ir,
    out vertices splineSegment::StemVertex verts[maxNumVerticesPerGroup],
    out indices uint3 tris[maxNumTrianglesPerGroup],
    out primitives splineSegment::StemPrimitive prims[maxNumTrianglesPerGroup]
)
{
    using namespace splineSegment;

    const DrawSegmentRecord segmentRecord = ir.Get();

    const int ringsPerGroup = segmentRecord.faceRingsPerGroup;

    float uPoints = lerp(segmentRecord.fromPoints, segmentRecord.toPoints, .5);

    int fromPointsI = RoundUpMultiple2(segmentRecord.fromPoints);
    int toPointsI   = RoundUpMultiple2(segmentRecord.toPoints);
    int uPointsI    = RoundUpMultiple2(uPoints);
    int vPointsI    = RoundUpMultiple2(segmentRecord.vPoints);

    bool isFirstGroup = groupOfSegment == 0;
    bool isLastGroup  = groupOfSegment == (segmentRecord.dispatchGrid - 1);


    int V = (ringsPerGroup + 1) * uPointsI;
    int T = ringsPerGroup * 2 * (uPointsI - 1);

    if(isLastGroup){
        int globalRings = vPointsI - 1;
        int ringsLastGroup = globalRings - (ringsPerGroup * (segmentRecord.dispatchGrid - 1));

        V = (ringsLastGroup + 1) * uPointsI;
        V += - uPointsI + toPointsI;
        T = ringsLastGroup * 2 * (uPointsI - 1);
        T += - (uPointsI - 1) + (toPointsI - 1);
    }

    if(isFirstGroup){
        V += - uPointsI + fromPointsI;
        T += - (uPointsI - 1) + (fromPointsI - 1);
    }

    int globalRingOffset     = groupOfSegment * ringsPerGroup;
    int globalVertexOffset   = isFirstGroup ? 0 : (fromPointsI                        + uPointsI * (globalRingOffset - 1));
    int globalTriangleOffset = isFirstGroup ? 0 : ((fromPointsI - 1) + (uPointsI - 1) + 2 * (uPointsI - 1) * (globalRingOffset - 1));

    SetMeshOutputCounts(V, T);

    const SegmentInfo si = segmentRecord.si;
    const TreeParameters params = GetTreeParameters();

    const StemTubeCage cage     = segmentRecord.cage.Decompress();

    // make gtid signed...
    int id = gtid;

    int lastRingVertexStart = fromPointsI + uPointsI * (vPointsI - 2);

    float dist = distance(cage.from.pos, GetCameraPosition());

    // write vertices
    if(id < V){
        int globalVertexId = globalVertexOffset + id;
        bool inFirstRing = (globalVertexId < fromPointsI);
        bool inLastRing  = (globalVertexId >= lastRingVertexStart);
        int ring = !inFirstRing + max(0, (globalVertexId - fromPointsI) / uPointsI);
        ring = min(ring, vPointsI - 1);

        int vertsBeforeRing = !inFirstRing * (fromPointsI + (ring - 1) * uPointsI);
        int inRing = globalVertexId - vertsBeforeRing;

        int pointsInRing = inFirstRing ? fromPointsI : inLastRing ? toPointsI : uPointsI;
        float pointsInRingf = inFirstRing ? segmentRecord.fromPoints : inLastRing ? segmentRecord.toPoints : uPoints;
        float u = SmoothTessellation(inRing, pointsInRingf, pointsInRing);

        float v = SmoothTessellation(ring, segmentRecord.vPoints, vPointsI);

        float3 splineCenter = StemSpline(cage.from.pos, qGetZ(cage.from.rot), cage.to.pos, qGetZ(cage.to.rot), v);

        float4 rot     = qSlerp(cage.from.rot, cage.to.rot, v);

        float3 toCam = GetCameraPosition() - splineCenter;

        float2 inPlane = qTransform(qConj(rot), toCam).xy;
        float  theta   = atan2(inPlane.y, inPlane.x);

        float openingAngle = lerp(segmentRecord.fromOpeningAngle, segmentRecord.toOpeningAngle, v);
        float angle = lerp(-openingAngle, openingAngle, u);

        float z = lerp(si.GetFromZ(), si.GetToZ(), v);
        float radius = r(segmentRecord.si, params, theta + angle, z, v);

        float3 offset = qGetX(qMul(rot, qRotateZ(theta + angle)));

        StemVertex vertex;
        {
            static const float tau = 2 * PI;
            int scale = max(1, int(PI * si.radius + 1.3));
            float2 uvu = float2((theta + angle) / PI, (si.length * z * 2)/(4 * si.radius));
            float2 uvWorld = si.radius * PI * uvu;
            float2 uv = scale * uvu;

            float2 cloudPos = uv * float2(8, 2);
            float cloud = CloudNoiseWrapX(cloudPos, scale * 8, 2);

            float o = WrappingXPerlinNoise(uv * 2, scale * 2);

            float voronoi = 0;
            static const float voronoiStart = 16;
            if(si.level < 2 && dist < voronoiStart){
                float v = VoronoiDistanceWrapX((uv + 0.03 * o) * float2(8, 1), scale * 8);
                v = min(v, params.StemBumpGapSize) * 4;
                voronoi = LerpIn(v, voronoiStart, voronoiStart * .8, dist);
            }
            float bump = lerp(cloud, voronoi, params.StemBumpVoronoiWeight);
            bump *= 1 + si.level;

            float y = offset.y;

            float d = abs(GetSeason() - 2);
            if(y > 0.5 && d > 1.75){
                bump = lerp(bump, y * 10, (y - 0.5) * 2 * (d-1.75) /.25);
            }
            radius *= 1 + bump * 0.12 * params.StemBumpStrength;
        }

        vertex.openingAngle_u_v_z = float4(angle, inPlane.yx, z);

        vertex.fromTrafo = segmentRecord.cage.from.trafo;
        vertex.toTrafo   = segmentRecord.cage.to.trafo;

        float3 worldSpacePosition = splineCenter + radius * offset;

        vertex.clipSpacePosition = mul(GetViewProjectionMatrix(), float4(worldSpacePosition, 1));

        float distance = segmentRecord.aoDistance + ((1-v) * si.length) / params.nCurveRes[si.level];
        vertex.ao = fakeAOfromDistance(distance);
        verts[id] = vertex;
    }

    int trianglesUntilSecondTriangleRing = fromPointsI - 1;
    int trianglesUntilLastTriangleRing   = (trianglesUntilSecondTriangleRing + (uPointsI - 1) + 2 * (uPointsI - 1) * (vPointsI - 3));
    int trianglesPerTriangleRing   = uPointsI - 1;
    int numRings = vPointsI - 1;

    if(id < T){
        int globalTriangleId = globalTriangleOffset + id;

        bool inFirstRing = globalTriangleId < trianglesUntilSecondTriangleRing;
        bool inLastRing  = globalTriangleId >= trianglesUntilLastTriangleRing;

        int triangleRing = inFirstRing ? 0 : 1 + (globalTriangleId - trianglesUntilSecondTriangleRing) / trianglesPerTriangleRing;
        triangleRing = min(triangleRing, 2 * vPointsI - 3);

        bool down = triangleRing & 1;

        int startVertex = inFirstRing ? 0 : max(0, fromPointsI + ((triangleRing-1) / 2) * uPointsI);
        int offset = (triangleRing < 2) ? fromPointsI : uPointsI;
        offset = down ? -offset : offset;

        int numTrianglesUntilRing = inFirstRing ? 0 : trianglesUntilSecondTriangleRing + (triangleRing - 1) * trianglesPerTriangleRing;
        int inTriangleRing = globalTriangleId - numTrianglesUntilRing;

        int ring = triangleRing / 2;

        int2 ab = uPointsI.xx;
        if(numRings == 1){
            ab.x = fromPointsI - 1;
            ab.y = toPointsI   - 1;
        }else if(ring == 0 && numRings > 1){
            ab.x = fromPointsI - 1;
            ab.y = uPointsI    - 1;
        }else if(ring == (numRings-1)){
            ab.x = uPointsI  - 1;
            ab.y = toPointsI - 1;
        }
        ab = down ? ab.yx : ab.xy;

        float bias = down ? .5001 : .4999;
        float t = saturate((inTriangleRing + bias) / (ab.x));
        int third = offset + int((ab.y) * t + .5);

        uint3 tri = startVertex + uint3(inTriangleRing, inTriangleRing + 1, third);

        tri.xy = down ? tri.yx : tri.xy;

        tri -= globalVertexOffset;

        tris[id]       = tri;
        prims[id].sip = PackSegmentInfo(si);
    }
}

float4 StemPixelShader(
    const in splineSegment::StemVertex vertex,
    const in splineSegment::StemPrimitive primitive,
    float3 barycentrics : SV_Barycentrics,
    bool isFrontFace : SV_IsFrontFace
) : SV_Target0
{
    using namespace splineSegment;

    SurfaceData surface;
    surface.baseColor.a  = 1;
    surface.metallic     = 0;
    surface.occlusion    = vertex.ao;
    surface.translucency = 0;

    SegmentInfo si = UnpackSegmentInfo(primitive.sip);

    const TreeParameters params = GetTreeParameters();

    float openingAngle = vertex.openingAngle_u_v_z.x;
    float2 tmp = normalize(vertex.openingAngle_u_v_z.yz);
    float centerAngle  = atan2(tmp.x, tmp.y);
    float theta = (2 * PI + centerAngle + openingAngle) % (2*PI);

    TreeTransformCompressedq32 fromTrafo;
    fromTrafo.trafo = GetAttributeAtVertex(vertex.fromTrafo, 2);

    TreeTransformCompressedq32 toTrafo;
    toTrafo.trafo = GetAttributeAtVertex(vertex.toTrafo, 2);

    StemTubeCage cage;
    cage.from.pos = fromTrafo.GetPos();
    cage.from.rot = fromTrafo.GetRot();
    cage.to.pos   = toTrafo.GetPos();
    cage.to.rot   = toTrafo.GetRot();
    float z = vertex.openingAngle_u_v_z.w;

    {
        float dist = distance(cage.from.pos, GetCameraPosition());
        const float t = (z - si.GetFromZ()) / (si.GetToZ() - si.GetFromZ());

        float4 rot = qSlerp(cage.from.rot, cage.to.rot, t);
        float3 offset = qGetX(qMul(rot, qRotateZ(theta)));
        float3 splinePos = StemSpline(cage.from.pos, qGetZ(cage.from.rot), cage.to.pos, qGetZ(cage.to.rot), t);
        float radius = r(si, params, theta, z, t);

        static const float tau = 2 * PI;
        int scale = max(1, int(PI * si.radius + 1.3));
        float2 uv = scale * float2(theta / PI, (si.length * z * 2)/(4 * si.radius));


        float2 cloudPos = uv * float2(8, 2);
        float cloud = CloudNoiseWrapX(cloudPos, scale * 8, -log2(length(ddx(cloudPos) + ddy(cloudPos))) + bumpLevelBias);

        float o = WrappingXPerlinNoise(uv * 2, scale * 2);


        float voronoi = 0;
        static const float voronoiStart = 32;
        if(si.level < 2 && dist < voronoiStart){
            float v = VoronoiDistanceWrapX((uv + 0.03 * o) * float2(8, 1), scale * 8);
            v = min(v, params.StemBumpGapSize) * 4;
            voronoi = LerpIn(v, voronoiStart, voronoiStart * .8, dist);
        }

        float bump = lerp(cloud, voronoi, params.StemBumpVoronoiWeight);
        bump *= 1 + si.level;

        float d = abs(GetSeason() - 2);
        bool isSnow = offset.y > 0.5 && d > 1.75;
        if(isSnow){
            bump = 0;
            surface.baseColor.rgb = float3(1,1,1);
        }else{
            // small branches are green
            static const float3 smallColor = params.StemSmallColor.rgb;
            static const float3 bigColor   = params.StemBigColor.rgb;
            static const float smallRadius = 0.001;
            static const float bigRadius = 0.025;
            const float l = saturate((si.radius - smallRadius) / (bigRadius - smallRadius));
            surface.baseColor.rgb = lerp(smallColor, bigColor, l);

            // spots
            if(dist < 32 && (Voronoi(uv * params.StemLichenFrequency) + 0.6 * cloud) < (params.StemLichenSize - .5) && bump > 0.3){
                surface.baseColor.rgb *= float3(1.5, 1.65, 1.5);
            }
        }

        if(params.StemBirchTexture && !isSnow){
            float2 pos = uv * float2(4, 1.25);
            float birch = CloudNoiseWrapX(pos, scale * 4, -log2(length(ddx(pos) + ddy(pos))) + bumpLevelBias);
            float thresh = (si.level == 0) ? (1-z - .6) : -.1;
            bool smooth = birch > thresh;

            if(smooth){
                bump *= 0.05;
                surface.baseColor.rgb = .5.xxx;
            }
        }

        radius *= 1 + bump * 0.12 * params.StemBumpStrength;
        surface.normal.xyz = Normal(splinePos + offset * radius);

        // roughness
        surface.roughness = (bump >= params.StemBumpGapSize) ? 0.6 : .9;
    }

    // uncomment and disable backface culling for visualization of single side tesselation
    //if(isFrontFace) discard;
    //if (!isFrontFace) {
    //    surface.baseColor.rgb = float3(1,0,0);
    //}

    return ShadeSurface(surface);
}
