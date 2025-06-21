#pragma once

#include "Quaternion.h"
#include "Shading.h"
#include "Camera.h"
#include "TreeModel.h"

static const int verticesPerLobe  = 16;
static const int trianglesPerLobe = 16;

static const int lobesPerGroup = min(128 / verticesPerLobe, 128 / trianglesPerLobe);

static const int maxNumVerticesPerLobeGroup  = lobesPerGroup * verticesPerLobe;
static const int maxNumTrianglesPerLobeGroup = lobesPerGroup * trianglesPerLobe;

static const int maxCoalescedDrawLeafRecords = 256;
static const int maxDrawLobeGroupsPerDispatch = (maxCoalescedDrawLeafRecords + lobesPerGroup - 1) / lobesPerGroup;

struct DrawLeafRecordBundle {
    uint2 dispatchGrid : SV_DispatchGrid;
    uint isBlossom : 1;
    uint leafCount : 31;
    DrawLeafRecord leaves[maxCoalescedDrawLeafRecords];
};

[Shader("node")]
[NodeLaunch("coalescing")]
[NodeId("CoalesceDrawLeaves", 0)]
[NumThreads(maxCoalescedDrawLeafRecords, 1, 1)]
void CoalesceDrawLeaves(
    uint gtid : SV_GroupThreadID,

    [MaxRecords(maxCoalescedDrawLeafRecords)]
    GroupNodeInputRecords<DrawLeafRecord> irs,

    [MaxRecords(1)]
    [NodeId("DrawLeafBundle")]
    NodeOutput<DrawLeafRecordBundle> output
){
    GroupNodeOutputRecords<DrawLeafRecordBundle> outputRecord = output.GetGroupNodeOutputRecords(1);

    const TreeParameters treeParams = GetTreeParameters();

    outputRecord.Get().dispatchGrid = uint2(DivideAndRoundUp(irs.Count(), lobesPerGroup), min(treeParams.Leaf.Lobes, TREE_MAX_LOBE_COUNT));
    outputRecord.Get().isBlossom    = 0;
    outputRecord.Get().leafCount    = irs.Count();

    if (gtid < irs.Count()) {
        outputRecord.Get().leaves[gtid] = irs.Get(gtid);
    }

    outputRecord.OutputComplete();
}

[Shader("node")]
[NodeLaunch("coalescing")]
[NodeId("CoalesceDrawLeaves", 1)]
[NumThreads(maxCoalescedDrawLeafRecords, 1, 1)]
void CoalesceDrawBlossom(
    uint gtid : SV_GroupThreadID,

    [MaxRecords(maxCoalescedDrawLeafRecords)]
    GroupNodeInputRecords<DrawLeafRecord> irs,

    [MaxRecords(1)]
    [NodeId("DrawLeafBundle")]
    NodeOutput<DrawLeafRecordBundle> output
){
    GroupNodeOutputRecords<DrawLeafRecordBundle> outputRecord = output.GetGroupNodeOutputRecords(1);

    const TreeParameters treeParams = GetTreeParameters();

    outputRecord.Get().dispatchGrid = uint2(DivideAndRoundUp(irs.Count(), lobesPerGroup), min(treeParams.Blossom.Lobes, TREE_MAX_LOBE_COUNT));
    outputRecord.Get().isBlossom    = 1;
    outputRecord.Get().leafCount    = irs.Count();

    if (gtid < irs.Count()) {
        outputRecord.Get().leaves[gtid] = irs.Get(gtid);
    }

    outputRecord.OutputComplete();
}

struct LeafVertex{
    float4 clipSpacePosition  : SV_POSITION;
    float3 worldSpacePosition : POSITION1;
    float2 texCoord           : TEXCOORD0;
    float2 lobeTexCoord       : TEXCOORD1;
    float  ao                 : TEXCOORD2;
    nointerpolation float4 worldSpaceNormal   : NORMAL0;
};

struct BlossomSeed {
    uint blossom : 1;
    uint seed    : 31;
};

struct LeafPrimitive {
    int  triangleType : BLENDINDICES0;
    uint blossomSeed  : BLENDINDICES1;
    uint lobe         : BLENDINDICES2;
};

float2 Rotate2D(float2 v, float angle){
    const float c = cos(angle);
    const float s = sin(angle);
    return float2(
        c * v.x - s * v.y,
        s * v.x + c * v.y);
}

[Shader("node")]
[NodeLaunch("mesh")]
[NodeId("DrawLeafBundle")]
[NodeMaxDispatchGrid(maxDrawLobeGroupsPerDispatch, TREE_MAX_LOBE_COUNT, 1)]
[NumThreads(128, 1, 1)]
[OutputTopology("triangle")]
void LeafMeshShader(
    uint2 gid : SV_GroupID,
    uint  gtid : SV_GroupThreadID,
    DispatchNodeInputRecord<DrawLeafRecordBundle> ir,
    out vertices LeafVertex verts[maxNumVerticesPerLobeGroup],
    out indices uint3 tris[maxNumTrianglesPerLobeGroup],
    out primitives LeafPrimitive prims[maxNumTrianglesPerLobeGroup]
){
    const bool isBlossom            = ir.Get().isBlossom;
    const TreeParameters treeParams = GetTreeParameters();
    const LeafParameters params     = GetLeafParameters(treeParams, isBlossom);

    const int startGlobalLeafId = gid.x * lobesPerGroup;

    const int localLeafCount = clamp(ir.Get().leafCount - int(gid.x) * lobesPerGroup, 0, lobesPerGroup);
    const int V = localLeafCount * verticesPerLobe;
    const int T = localLeafCount * trianglesPerLobe;

    SetMeshOutputCounts(V, T);

    LeafVertex vertex;

    if (gtid < V)
    {
        const int vertId = gtid;

        const int globalLeafId = startGlobalLeafId + vertId / verticesPerLobe;
        const int inLeaf = vertId % verticesPerLobe;

        const bool isLeft = inLeaf > 8;
        const int inHalf = (isLeft) ? (16 - inLeaf) : inLeaf;

        const DrawLeafRecord leafRecord = ir.Get().leaves[(globalLeafId)];

        const float4 rot = leafRecord.trafo.GetRot();

        const int  lobe = isBlossom? 0 : gid.y;

        float botAngle = radians(params.BotAngle);
        float midAngle = radians(params.MidAngle);
        float topAngle = radians(params.TopAngle) + 0.2 * random::SignedRandom(leafRecord.seed, 222);

        const float3x2 p = {
            float2(0, 0),
            float2(-.5 + 0.1 * random::SignedRandom(leafRecord.seed, 92), params.SideOffset + 0.1 * random::SignedRandom(leafRecord.seed, 29)),
            float2(0, 1)
        };
        const float3x2 t = {
            float2(sin(botAngle), cos(botAngle)),
            float2(sin(midAngle), cos(midAngle)),
            float2(sin(topAngle), cos(topAngle))
        };
        float4x4 w = {
            float4(  1,     0,   0,     0),
            float4(  1,  0.25,   0,     0),
            float4(0.5, 0.125, 0.5, 0.125),
            float4(  0,     0,   1,  0.25)
        };

        int isSecond = (inHalf > 3);
        const float l = isSecond ? distance(p[1], p[2]) : distance(p[0], p[1]);
        isSecond += (inHalf > 7);

        const float4x2 cp = float4x2(
                p[isSecond + 0],
                t[min(isSecond + 0, 2)] * l,
                p[min(isSecond + 1, 2)],
                t[min(isSecond + 1, 2)] * -l
        );
        float3 modelSpacePosition;
        modelSpacePosition.y = 0.;
        modelSpacePosition.xz = mul(w[inHalf % 4], cp);

        if(isLeft) modelSpacePosition.x = -modelSpacePosition.x;

        float centerLobeF = (params.Lobes - 1) / 2.;
        int centerLobe = centerLobeF;

        float lobeScale = 1 - params.LobeFalloff * abs(lobe - centerLobeF);

        vertex.lobeTexCoord = modelSpacePosition.xz;
        vertex.lobeTexCoord.x *= params.ScaleX;

        modelSpacePosition *= leafRecord.scale * lobeScale;
        modelSpacePosition.x *= params.ScaleX;

        if (isBlossom) {
            const uint  blossomLeaf = gid.y;

            const float blossomLeafAngleStep = 2 * PI / float(params.Lobes);
            const float blossomLeafAngle     = blossomLeaf * blossomLeafAngleStep;

            float angle = blossomLeafAngleStep / 4.f;
            angle = (modelSpacePosition.x > 0) ? angle : -angle;

            const float4 blossomRotation =
                qMul(qRotateZ(blossomLeafAngle),
                     qMul(qRotateX(radians(params.LobeAngle)), qRotateZ(angle)));
            modelSpacePosition = qTransform(blossomRotation, modelSpacePosition);

            vertex.worldSpaceNormal.xyz = qGetY(qMul(rot, blossomRotation));
        } else {
            float angle = 0;

            if (params.IsNeedle) {
                int inBlade = inLeaf % 4;
                modelSpacePosition.x = .25 * params.ScaleX * ((inBlade & 1) ? 1 : -1);
                modelSpacePosition.y = 0;
                modelSpacePosition.z = inBlade >> 1;

                vertex.lobeTexCoord = modelSpacePosition.xz;
                modelSpacePosition *= leafRecord.scale * lobeScale;
                angle = 0.75 * PI * (inLeaf / 4);
            } else {
                angle = max(0, GetSeason() - 2) * radians(20);
                angle = (modelSpacePosition.x > 0) ? angle : -angle;
            }

            // Needle rotation or leaf folding in fall
            modelSpacePosition.xy = Rotate2D(modelSpacePosition.xy, angle);
            vertex.worldSpaceNormal.xyz = qGetY(qMul(rot, qRotateZ(angle)));

            // lobe rotation
            modelSpacePosition.xz = Rotate2D(modelSpacePosition.xz, radians(-params.LobeAngle * centerLobeF + params.LobeAngle * lobe));

            if (!params.IsNeedle) {
                if(lobe < centerLobe){
                    modelSpacePosition.x = max(0, modelSpacePosition.x);
                }else if(lobe > centerLobe){
                    modelSpacePosition.x = min(0, modelSpacePosition.x);
                }
            }
        }

        modelSpacePosition.y -= abs(vertex.lobeTexCoord.x / lobeScale * 0.001);

        vertex.texCoord = modelSpacePosition.xz;

        float3 worldSpacePosition = leafRecord.trafo.GetPos() + qTransform(rot, modelSpacePosition);

        vertex.worldSpacePosition.xyz = worldSpacePosition;

        vertex.clipSpacePosition = mul(GetViewProjectionMatrix(), float4(worldSpacePosition, 1));

        vertex.ao = fakeAOfromDistance(leafRecord.aoDistance);

        verts[vertId] = vertex;
    }

    if (gtid < T) {
        const int triId = gtid;

        const int inLeaf      = triId % trianglesPerLobe;
        const int localLeafId = triId / trianglesPerLobe;
        const int globalLeafId = startGlobalLeafId + localLeafId;

        const int inHalf = inLeaf / 2;
        const bool isLeft = inLeaf % 2;

        const DrawLeafRecord leafRecord = ir.Get().leaves[(globalLeafId)];

        const bool topIsConvex = params.TopConvex;
        const bool isNeedle    = params.IsNeedle;

        LeafPrimitive primitive;

        static const uint3 buffer[8] = {
            uint3(0,2,4),
            uint3(0,4,6),
            uint3(0,6,7),
            uint3(0,7,8),
            uint3(0,1,2),
            uint3(2,3,4),
            uint3(4,5,6),
            uint3(6,7,8),
        };

        uint3 tri = buffer[inHalf];
        primitive.triangleType = (inHalf > 3) ? 1 : 0;

        if(topIsConvex){
            if(inHalf == 2){
                tri.z += 1;
            }else if(inHalf == 3){
                tri = uint3(0,0,0);
            }
        }else if(inHalf == 7){
            tri = tri.zyx;
            primitive.triangleType = -1;
        }

        if(isLeft){
            tri = (16 - tri) * (tri > 0);
            tri = tri.zyx;
        }

        // In theory, a smart compiler could detect this as a single move...
        BlossomSeed blossomSeed;
        blossomSeed.blossom = isBlossom;
        blossomSeed.seed    = leafRecord.seed;

        primitive.blossomSeed = (uint)blossomSeed;
        primitive.lobe        = isBlossom? 0 : gid.y;

        if(isNeedle){
            int blade = inLeaf / 2;
            int inBlade = inLeaf % 2;
            tri = blade * 4 + ((inBlade == 0) ? uint3(0,1,2) : uint3(1,3,2));
            if(blade > 3) tri = uint3(0,0,0);
            primitive.triangleType = 0;
        }

        tris[triId] = localLeafId * verticesPerLobe + tri;

        prims[triId] = primitive;
    }
}

float2 computeUV(const float3 bary){
    return float2(mad(bary.y,.5f, bary.z),  bary.z);
}
float computeQuadraticBezierFunction(const float2 uv){
    return uv.x * uv.x - uv.y;
}
template<typename T>
int map01toN1P1(in T x) {
  return 2 * x - 1;
}

float PointLineDistance(const float2 lineStart,const float angle,const float curve,const float2 pnt){        
    return cos(angle)*(lineStart.y-pnt.y)-pow(sin(angle)*(lineStart.x-pnt.x), curve);    //Source wikipedia ;-)
}
    
float2 PointLineDerivative(const float2 lineStart, const  float angle, const float curve, const float2 pnt){
    const float x = curve * sin(angle)*pow(sin(angle)*(lineStart.x-pnt.x),curve-1.);
    const float y = -cos(angle);
    return float2(x,y);
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

float VoronoiDistance(in float2 x)
{
    float2 ip = floor(x);
    float2 fp = frac(x);

    //----------------------------------
    // first pass: regular voronoi
    //----------------------------------
	float2 mg, mr;
    float2 minPoint;
    float md = 8.0;
    for( int j=-1; j<=1; j++ )
    for( int i=-1; i<=1; i++ )
    {
        float2 g = float2(float(i),float(j));
		float2 o = hash2( ip + g );
        float2 r = g + o - fp;
        float d = dot(r,r);

        if(d < md)
        {
            md = d;
            mr = r;
            mg = g;
            minPoint = r;
        }
    }

    //----------------------------------
    // second pass: distance to borders
    //----------------------------------
    md = 8.0;
    for( int j=-2; j<=2; j++ )
    for( int i=-2; i<=2; i++ )
    {
        float2 g = mg + float2(float(i),float(j));
		float2 o = hash2( ip + g );
        float2 r = g + o - fp;

        if( dot(mr-r,mr-r)>0.00001 ){
            md = min( md, dot( 0.5*(mr+r),normalize(r-mr)));
        }
    }        
    return md;
}

float4 LeafPixelShader(
    const in LeafVertex vertex, 
    const in LeafPrimitive primitive, 
    const in float3 barycentrics : SV_Barycentrics, 
    const in bool isFrontFace : SV_IsFrontFace
) : SV_Target0
{
    SurfaceData surface;

    if(primitive.triangleType != 0){
        bool isConvex = primitive.triangleType > 0;
        float2 uv = computeUV(barycentrics);
        float  y  = computeQuadraticBezierFunction(uv);        
        y += 0.05 * random::PerlinNoise2D(128*vertex.texCoord);    
        if((y < 0) ^ isConvex) discard;
    }

    BlossomSeed blossomSeed = (BlossomSeed)primitive.blossomSeed;
  
    const LeafParameters params = GetLeafParameters(GetTreeParameters(), blossomSeed.blossom);
    
    surface.metallic     = 0;
    surface.roughness    = 0.8;
    surface.occlusion    = vertex.ao;
    surface.translucency = params.Translucency;
    surface.baseColor.a  = 1;

    const float LeafStrandAngle = radians(params.StrandAngle);

    float2 p = vertex.lobeTexCoord;

    p.x += 0.01 * random::PerlinNoise2D(8 * p + float(blossomSeed.seed & 0xFF));
    p.y += 0.025 * random::PerlinNoise2D(4 * p - float(blossomSeed.seed & 0xFF));

    float distance = abs(p.x) / params.StrandCenterThickness;

    float strandStep = params.StrandStep;    
    strandStep *= params.IsNeedle ? 0.5 : 1;

    float offset = (p.x > 0) ? strandStep * params.StrandOffset : 0;

    const float cAngle = cos(LeafStrandAngle);
    const float xPart = pow(sin(LeafStrandAngle) * abs(p.x), params.StrandCurve) / cAngle;

    int closestIdx = round((p.y + xPart - offset) / strandStep);

    float randomOffset = params.IsNeedle ? random::SignedRandom(closestIdx, blossomSeed.seed) : 0;

    float closest = closestIdx * strandStep + offset + 0.35 * strandStep * randomOffset;

    float m = abs(closest - (p.y + xPart));

    float thickness = params.StrandThickness;
    thickness *= params.IsNeedle ? 4 : 1;
    distance = min(distance, m * -cAngle / thickness);
        
    float3 wsn = normalize(GetAttributeAtVertex(vertex.worldSpaceNormal, 1).xyz);

    if(!isFrontFace && params.IsNeedle) wsn = -wsn;

    float xs = distance * 1000;
    float bump = params.StrandStrength * -0.0002 * exp(-(xs*xs));
    //if(bump > -0.000001){
    //    bump -= 0.001 * params.CellStrength * max(0.01, (0.1-VoronoiDistance(params.CellScale * vertex.texCoord)));        
    //}
            
    float3 pos = vertex.worldSpacePosition + wsn * (bump * !blossomSeed.blossom);
    float3 bumpedNormal = Normal(pos);
    surface.normal = bumpedNormal;

    float season = GetNoisedLeafSeason(blossomSeed.seed) + params.SeasonOffset;
    season += 0.5 * -(vertex.ao - .5);

    surface.baseColor.rgb = GetSeasonLeafColor(params, season, blossomSeed.blossom);

    if(params.IsNeedle){
        const float needleSpacing    = 0.001f;
        const float shapeScale       = 1.f;
        
        surface.baseColor.rgb += max(0, (abs(2 - GetSeason())-1.5)) * 32 * abs(vertex.lobeTexCoord.x);
        bool outsideShape = (1 - (p.y * shapeScale)) < abs(pow(10*abs(p.x), 6));
        if(outsideShape) discard;
        if(distance > needleSpacing) discard;
    }else if(params.StrandStrength*distance < 0.0005 && !blossomSeed.blossom){
        surface.baseColor.rgb = .25 * float3(.85,.82,.28);
    }

    return ShadeSurface(surface);
}