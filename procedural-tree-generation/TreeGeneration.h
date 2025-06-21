#pragma once

#include "Records.h"
#include "TreeModel.h"
#include "Camera.h"
#include "LeafDensity.h"
#include "SplineTessellation.h"


// ============================ Generation Functions ======================

// Weber-Penn Section 4.1
float GetStemCurve(in const TreeParameters params, in const uint level, in const float curveResolution, in const uint segmentSeed, in const float z){
    // NOT IMPLEMENTED: Helix

    float rotateX = (params.nCurveV[level] / curveResolution) * random::SignedRandom(segmentSeed, 6);
    if(params.nCurveBack[level] == 0){
        rotateX += params.nCurve[level] / curveResolution;
    }else{
        const float resh = curveResolution / 2;
        const float alpha = MapRange(z, 0.4f, 0.6f, 0.f, 1.f);
        const float curve = lerp(params.nCurve[level], params.nCurveBack[level], alpha);
        rotateX += curve/resh;
    }
    return rotateX;
}

// Weber-Penn Section 4.2
void AddSplitSpread(in const SegmentInfo si, in const TreeParameters params, in const uint segmentSeed, in const int step, inout float4 rot, inout float splitCorrection){
    float declination = degrees(acos(qGetZ(rot).y));
    float splitAngle = params.nSplitAngle[si.level] + params.nSplitAngleV[si.level] * random::SignedRandom(segmentSeed, 0xFA) - declination;
    splitAngle = max(0, splitAngle);

    int remainingSegments = params.nCurveRes[si.level] - step - 1;
    splitCorrection -= splitAngle / remainingSegments;

    rot = qMul(rot, qRotateX(radians(splitAngle)));

    declination = degrees(acos(qGetZ(rot).y));
    float spreadAngle = 20 + 0.75 * (30 + abs(declination - 90));
    float r = random::Random(segmentSeed, 900);
    spreadAngle *= r * r;
    if(random::Random(segmentSeed, step, 2) < .5) spreadAngle = -spreadAngle;

    rot = qMul(qRotateY(radians(spreadAngle)), rot);
}

// Weber-Penn Section 4.8
void AddVerticalAttraction(in const int level, in const TreeParameters params, in float curveResolution, inout float4 rot){
    float attractionUp = 0;

    if(level > 1) attractionUp += LoadPersistentConfigFloat(PersistentConfig::TREE_ATTRACTION_UP);

    float3 axis = qGetZ(rot);
    float declination = acos(axis.y);

    static const float t = 0.2;

    const float d = 2 - abs(GetSeason() - 2);
    attractionUp += min(0, d*d/t-t);

    if(level > 0 && params.Fruit.Chance > 0){
        float level_factor = 1. / pow(4., params.Levels - level - 1);
        float fruitProgress = GetGeneralSeasonFruitProgress(GetSeason());
        if(GetSeason() > 3.2){
            fruitProgress = MapRange<float>(GetSeason(), 3.2, 3.3, 1, 0);
        }
        const float fruitScale = GetFruitScale(fruitProgress);
        float size = 15 * params.Fruit.Size * fruitScale;
        float volume = size * size * size;
        // weight should grow cubic with the size
        attractionUp -= level_factor * params.Blossom.Count * params.Fruit.Chance * volume;
    }

    if(attractionUp != 0 && axis.y < .9999){
        float c = saturate(MapRange<float>(axis.y, 1, .95, 0, 1)); // -1 dot problem correction
        float curveUpSegment = attractionUp * abs(declination * sin(declination)) / curveResolution;
        axis = normalize(float3(-axis.z, 0, axis.x));
        if(any(axis != 0)){
            rot = qMul(qRotateAxisAngle(axis, curveUpSegment*c), rot);
        }
    }
}

float2 GetWindBend(in const SegmentInfo si, in const TreeParameters params, in const int step){
    static const float wind_speed = .5;
    static const float wind_gust = .25;

    const float z = float(step) / params.nCurveRes[si.level];

    float a0 = 4 * si.length * (1-z) / GetTaperedRadius(si, params, z);
    float a1 = wind_speed / 50 * a0;
    float a2 = wind_gust  / 50 * a0 + a1 * .5;
    a1 = radians(a1);
    a2 = radians(a2);

    return float2(a1, a2);
}

// Weber-Penn Section 4.7
void AddWindSway(in const float radius, in const float length, in const int curveRes, in const float3 position, inout float4 rot){
    float dir = GetWindDirection();
    float3 windDir = float3(-cos(dir),0,sin(dir));

    float3 zAxis = qGetZ(rot);
    float d = dot(zAxis, windDir);
    float fullAngle = acos(d);
    float3 rotAxis = normalize(cross(zAxis, windDir));

    float c = saturate(MapRange<float>(d, -1, -.95, 0, 1)); // -1 dot problem correction

    float angle = fullAngle * GetWindStrength() * 0.003 / max(radius, 0.03) * c / curveRes;

    float frequency = 2 / sqrt(length);
    frequency = min(frequency, 5);
    float forcePhase = 0.4 + random::PerlinNoise2D(position.xz * .75 - Time * frequency * windDir.xz * max(.8, GetWindStrength() * .06) );

    float4 r = qRotateAxisAngle(rotAxis, angle * forcePhase);
    rot = qMul(r, rot);
}

// Weber-Penn Section 4.3
float4 GetChildDownRotation(in const SegmentInfo si, in const TreeParameters params, in const uint seed, in const float ratio){
    const int nextLevelClamped = min(si.level + 1, 3);

    float downAngleV = params.nDownAngleV[nextLevelClamped];
    float downAngle = downAngleV * random::Random(seed, 1998);
    if(asuint(downAngleV) & 0x80000000U) {
        downAngle *= (1 - 2 * ShapeRatio(0, ratio));
    }
    downAngle += params.nDownAngle[nextLevelClamped];
    return qRotateX(radians(downAngle));
}

// Weber-Penn Section 4.3
float GetChildparentZAngle(in const SegmentInfo si, in const TreeParameters params, in const uint seed, in const int branchIdx){
    const int nextLevelClamped = min(si.level + 1, 3);

    const float rotate = params.nRotate[nextLevelClamped];
    const float rotateV = params.nRotateV[nextLevelClamped];
    float theta = 0;
    if(!(asuint(rotate) & 0x80000000U)) {
        float angle = rotateV * random::SignedRandom(seed, 50);
        // FIXME - this is does not account for the random rotation of the previous branches, do we care?
        float angleSum = branchIdx * rotate + angle;
        return radians(angleSum);
    }
    return BitSign(branchIdx, 0) * radians(180 - rotate + rotateV * random::SignedRandom(seed, 50));
}

void AddFruitWeight(inout float4 childRotation, in const float p){
    float3 childZ = qGetZ(childRotation);
    if(childZ.y != -1){
        float3 axis = float3(childZ.z, 0, -childZ.x); // equivalent to cross(childZ, float3(0, -1, 0))
        float angle = acos(clamp(-childZ.y, -1, 1));  // equivalent to acos(clamp(dot(childZ, float3(0, -1, 0)), ...
        float4 r = qRotateAxisAngle(axis, angle * p);
        childRotation = qMul(r, childRotation);
    }
}



static const int  maxChildRecords = 128;
static const int  maxSegmentRecords = 128;
#if USING_SOFTWARE_ADAPTER
// WARP is optimized for WaveSize(4), thus as we need wave intrinsics for the splitting/cloning of branches
// we limit the number of clones to this lower wave size.
static const uint maxClones = 4;
#else
static const uint maxClones = 32;
#endif

static const uint StemThreadGroupSize = maxClones;

groupshared TreeTransform groupCloneTrafo[maxClones];
groupshared TreeTransform groupClonePreTrafo[maxClones];

[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeMaxRecursionDepth(3)]
[NodeDispatchGrid(1, 1, 1)]
[NumThreads(StemThreadGroupSize, 1, 1)]
void Stem(
    uint gtid : SV_GroupThreadId,
    DispatchNodeInputRecord<GenerateTreeRecord> ir,

    [MaxRecords(maxChildRecords)]
    [NodeId("Stem")]
    NodeOutput<GenerateTreeRecord> generateTreeOutput,

    [MaxRecordsSharedWith(generateTreeOutput)]
    [NodeId("CoalesceDrawLeaves")]
    [NodeArraySize(3)]
    NodeOutputArray<DrawLeafRecord> drawLeafOutput,

    [MaxRecords(maxSegmentRecords)]
    [NodeId("DrawSegment")]
    NodeOutput<DrawSegmentRecord> drawSegmentOutput
)
{
    const GenerateTreeRecord inputRecord = ir.Get();
    const TreeParameters     params      = GetTreeParameters();

    SegmentInfo si;
    si.level        = 3 - GetRemainingRecursionLevels();
    si.length       = inputRecord.length;
    si.radius       = inputRecord.radius;
    si.fromZ        = 0;
    si.toZ          = 0;

    const float lengthBase       = params.nBaseSize[0] * inputRecord.scale;
    const int   nextLevel        = si.level + 1;
    const int   nextLevelClamped = min(nextLevel, params.Levels - 1);

    const float distanceToCamera = distance(GetCameraPosition(), inputRecord.trafo.GetPos());

    // Constants
    const float  curveResolution = clamp(params.nCurveRes[si.level], 1, 32);
    const uint   stemSeed        = inputRecord.seed;
    const float  segmentSplits   = params.nSegSplits[si.level];
    // random angles in radians for wind sway
    const float2 swayOffset      = float2(random::SignedRandom(inputRecord.seed, 'x'), random::SignedRandom(inputRecord.seed, 'y')) * PI;

    const uint children = min(inputRecord.children, maxChildRecords);
    float childDensity  = 1.f;
    float childScale    = 1.f;

    if (si.level == (params.Levels - 1)) {
        // We reduce the childDensity, i.e., number of leaves (children in last level) based on the distance to camera.
        // To compensate, we increase the size of the leaves.
        ComputeChildDensityAndScale(distanceToCamera, childDensity, childScale);
    }

    const float zoffset         = params.nBaseSize[si.level];
    const float childStepfDelta = (curveResolution * (1. - zoffset)) / float(children);
    const float firstChildStepf = curveResolution * zoffset + childStepfDelta * .5;

    const float3 startPos = inputRecord.trafo.GetPos();

    // State of current clone
    TreeTransform trafo;
    trafo.SetPos(startPos);
    trafo.SetRot(inputRecord.trafo.GetRot());

    float splitCorrection     = 0;
    int   cloneIndex          = 0;
    int   threadIndexInClone  = gtid;
    int   threadCountInClone  = StemThreadGroupSize;

    // Global state
    float splitError       = -params.nSegSplitBaseOffset[si.level];
    uint  drawOutputCount  = 0;
    uint  childOutputCount = 0;

    for (int step = 0; step < curveResolution; ++step) {
        // update from with encoded value
        si.fromZ = si.toZ;
        si.SetToZ((step + 1) / curveResolution);

        const float stepTSize = (si.GetToZ() - si.GetFromZ()) * curveResolution;

        // Weber-Penn Section 4.2: Stage 1: compute splits and update thread allocations
        bool hasSplits = false;
        int numSplits        = 1;
        bool isOriginalClone = false;

        {
            numSplits = max(0, round(segmentSplits + splitError));

            // Add base splits for first level
            if (step == 0) {
                numSplits += params.nBaseSplits[si.level];
            }

            // update error metric
            splitError -= numSplits - segmentSplits;

            // add current active branch to splits (numSplits = 1 means no splits)
            numSplits += 1;
        }

        hasSplits = numSplits > 1;

        // Handle splits
        if (hasSplits) {
            const uint threadsPerSplit = max(1, threadCountInClone / numSplits);
            // number of threads that cannot be evenly distributed; all remain with original branch
            const uint threadSurplus   = max(int(threadCountInClone) - int(numSplits * threadsPerSplit), 0);

            isOriginalClone = threadIndexInClone < (threadsPerSplit + threadSurplus);

            // Update thread in index and thread count per clone
            if (isOriginalClone) {
                threadCountInClone = threadsPerSplit + threadSurplus;
            } else {
                // Compute new index in branch
                threadIndexInClone = (threadIndexInClone - (threadsPerSplit + threadSurplus)) % threadsPerSplit;
                threadCountInClone = threadsPerSplit;
            }

            // prefix sum over last thread per clone, i.e. number of clones
            cloneIndex = WavePrefixCountBits(threadIndexInClone == (threadCountInClone - 1));
        }

        // Compute seeds for random values
        const uint stepSeed    = random::CombineSeed(stemSeed, step);
        const uint segmentSeed = random::CombineSeed(stepSeed, cloneIndex);

        if (threadIndexInClone == 0) {
            groupClonePreTrafo[cloneIndex] = trafo;
        }

        // Weber-Penn Section 4.1
        if(params.nCurveV[si.level] >= 0){
            const float stemCurve = GetStemCurve(params, si.level, curveResolution, segmentSeed, si.GetFromZ());
            trafo.rot     = qMul(trafo.rot, qRotateX(radians(stemCurve + splitCorrection)));
        }else{ // helix
            trafo.rot     = qMul(trafo.rot, qRotateX(radians(abs(params.nCurveV[si.level])) / curveResolution));
            trafo.rot     = qMul(trafo.rot, qRotateZ(radians(360) / curveResolution));
        }

        // Weber-Penn Section 4.2: Stage 2: apply splits/clones
        if (hasSplits && !isOriginalClone) {
            // update transform
            AddSplitSpread(si, params, segmentSeed, step, /* inout */ trafo.rot, /* inout */ splitCorrection);
        }

        // Weber-Penn Section 4.8
        {
            AddVerticalAttraction(si.level, params, curveResolution, trafo.rot);
        }

        // Weber-Penn Section 4.7
        if(!hasSplits){
            AddWindSway(si.radius, si.length, curveResolution, startPos, trafo.rot);
        }

        const float segmentLength = (si.GetToZ() - si.GetFromZ()) * si.length;
        trafo.pos += qGetZ(trafo.rot) * segmentLength;

        if (threadIndexInClone == 0) {
            groupCloneTrafo[cloneIndex] = trafo;
        }

        const bool threadActive  = threadIndexInClone == 0;
        const bool hasDrawOutput = threadActive && ((drawOutputCount + WavePrefixCountBits(threadActive)) < maxSegmentRecords);

        // Count all outputs
        drawOutputCount += WaveActiveCountBits(hasDrawOutput);

        {
            SegmentTessellationData tessellationData = (SegmentTessellationData)0;
            tessellationData.threadGroupCount        = 0;

            if (hasDrawOutput) {
                // Increase pixels per triangle with distance
                const float resolutionScale = MapRange(distanceToCamera, 30.f, 60.f, 1.f, 4.f);
#if USING_SOFTWARE_ADAPTER
                const float pixelsPerTriangle = 16.f;
#else
                const float pixelsPerTriangle = 4.f;
#endif

                tessellationData = ComputeVisibilityAndTessellationData(
                    si,
                    params,
                    groupClonePreTrafo[cloneIndex],
                    groupCloneTrafo[cloneIndex],
                    pixelsPerTriangle * resolutionScale);
            }

            const bool hasVisibleDrawOutput = hasDrawOutput && (tessellationData.threadGroupCount > 0);

            ThreadNodeOutputRecords<DrawSegmentRecord> drawSegmentOutputRecord =
                drawSegmentOutput.GetThreadNodeOutputRecords(hasVisibleDrawOutput);

            if (hasVisibleDrawOutput) {
                drawSegmentOutputRecord.Get().cage.from.SetPos(groupClonePreTrafo[cloneIndex].GetPos());
                drawSegmentOutputRecord.Get().cage.from.SetRot(groupClonePreTrafo[cloneIndex].GetRot());

                drawSegmentOutputRecord.Get().cage.to.SetPos(trafo.GetPos());
                drawSegmentOutputRecord.Get().cage.to.SetRot(trafo.GetRot());

                drawSegmentOutputRecord.Get().si            = si;
                drawSegmentOutputRecord.Get().aoDistance    = inputRecord.aoDistance + si.length - segmentLength * step;

                drawSegmentOutputRecord.Get().fromPoints        = tessellationData.fromPoints;
                drawSegmentOutputRecord.Get().toPoints          = tessellationData.toPoints;
                drawSegmentOutputRecord.Get().vPoints           = tessellationData.vPoints;
                drawSegmentOutputRecord.Get().faceRingsPerGroup = tessellationData.faceRingsPerGroup;
                drawSegmentOutputRecord.Get().fromOpeningAngle  = tessellationData.fromOpeningAngle;
                drawSegmentOutputRecord.Get().toOpeningAngle    = tessellationData.toOpeningAngle;
                drawSegmentOutputRecord.Get().dispatchGrid      = tessellationData.threadGroupCount;
            }

            drawSegmentOutputRecord.OutputComplete();
        }

        const uint activeClones    = WaveActiveCountBits(hasDrawOutput);
#if USING_SOFTWARE_ADAPTER
        // Up to 128 children, computed by 4 threads on WARP => max 32 iterations
        const uint childIterations = 32;
#else
        // Up to 128 children, computed by 32 threads => max 4 iterations
        const uint childIterations = 4;
#endif

        for (uint childIteration = 0; childIteration < childIterations; ++childIteration) {
            uint  stemChildIndex;
            float stemChildScale;
            // We scale leaves to zero and cull them based on the child density.
            // This helper computes the index and scale of the n-th child with scale > 0.
            // This saves computing children with 0 scale.
            GetNthChildIndexAndScale(childOutputCount + gtid, children, childDensity, stemChildIndex, stemChildScale);

            const float localChildStepf = firstChildStepf + stemChildIndex * childStepfDelta;
            const float t               = frac(localChildStepf) / stepTSize;
            // Quantize z as an easy fix to floating point errors when scaling curve res
            const float z               = SegmentInfo::DecodeZ(SegmentInfo::EncodeZ(localChildStepf / curveResolution));

            // Output child if...
            const bool hasChildInStep = 
                // ... the child is not scaled to zero
                (stemChildScale != 0.f) && 
                // ... and the child is in the current step
                (floor(localChildStepf) == step) && 
                // ... and we have not exceeded the maximum number of children
                ((childOutputCount + gtid) < children);
            bool       hasChildOutput = hasChildInStep;

            // End loop if no more children are generated
            if (WaveActiveAllTrue(!hasChildInStep)) {
                break;
            }

            // Housekeeping of maximum child count
            const uint childrenInStep = WaveActiveCountBits(hasChildInStep);
            childOutputCount += childrenInStep;

            const uint childCloneIndex = stemChildIndex % activeClones;
            const uint childSeed = random::CombineSeed(stemChildIndex, inputRecord.seed) & 0x7FFFFF;

            const float ratio = (si.length * (1-z))/(si.length - lengthBase);

            const float4 downAngleRot    = GetChildDownRotation(si, params, childSeed, ratio);
            const float  radiusParent    = GetTaperedRadius(si, params, z);
            const float  parentZRotAngle = GetChildparentZAngle(si, params, childSeed, stemChildIndex);

            // Compute child position along current spline segment
            const float3 childPosition = StemSpline(groupClonePreTrafo[childCloneIndex].pos,
                                                    qGetZ(groupClonePreTrafo[childCloneIndex].rot),
                                                    groupCloneTrafo[childCloneIndex].pos,
                                                    qGetZ(groupCloneTrafo[childCloneIndex].rot),
                                                    t);
            // Compute rotation along current spline segment
            const float4 rotParent = qSlerp(groupClonePreTrafo[childCloneIndex].rot, groupCloneTrafo[childCloneIndex].rot, t);
            const float3 parentZ   = qGetZ(rotParent);

            if (si.level == (params.Levels - 1)) {
                // Last level, output leaves

                const bool  isLeafBlossom = IsLeafBlossom(params, childSeed);
                const float fruitProgress = GetSeasonFruitProgress(childSeed);
                const float fruitScale    = GetFruitScale(fruitProgress);
                const bool  isFruit       = isLeafBlossom && (random::Random(childSeed, stemChildIndex) < params.Fruit.Chance) && fruitProgress > 0;

                const LeafParameters leafParams = GetLeafParameters(params, isLeafBlossom);

                // Compute scale based on child index
                const float baseScale = leafParams.Scale * ShapeRatio(leafParams.ScaleShape, 1.f - z) * childScale * stemChildScale;
                const float scale = leafParams.IsNeedle ? 
                    // Needles are not scaled by season
                    baseScale :
                    (
                        isFruit ? 
                            childScale * stemChildScale * params.Fruit.Size * fruitScale :
                            baseScale * GetSeasonLeafScale(childSeed, isLeafBlossom)
                    );

                // Compute child transform
                TreeTransformCompressedq32 childTransform;
                
                float4 childRotation = qMul(qRotateAxisAngle(parentZ, parentZRotAngle), qMul(rotParent, downAngleRot));
                
                // Apply gravity to fruit
                if (isFruit) {
                    AddFruitWeight(childRotation, params.Fruit.DownForce * fruitScale);
                }
                
                // Compute child position on surface of spline segment
                const float3 childZ = qGetZ(childRotation);
                const float  d      = clamp(dot(childZ, parentZ), .05, .95);
                const float3 offset = childZ * (radiusParent / sqrt(1 - d * d) + leafParams.StemLen);
                childTransform.SetPos(childPosition + offset);

                // Apply wind animation to leaves
                if (!isFruit) {
                    AddWindSway(0.03, leafParams.Scale, 1, childTransform.GetPos(), childRotation);
                }

                childTransform.SetRot(childRotation);
                
                // drawLeafOutput is node array with 3 nodes:
                // 0 = leaf
                // 1 = blossom
                // 2 = fruit
                uint childOutputArrayIndex = isLeafBlossom? (isFruit? 2 : 1) : 0;

                // Skip output if scale is zero
                hasChildOutput = hasChildOutput && (scale > 0.f);

                ThreadNodeOutputRecords<DrawLeafRecord> childOutputRecord = 
                    drawLeafOutput[childOutputArrayIndex].GetThreadNodeOutputRecords(hasChildOutput);

                if(hasChildOutput) {
                    childOutputRecord.Get().trafo      = childTransform;
                    childOutputRecord.Get().seed       = childSeed;
                    childOutputRecord.Get().scale      = scale;
                    childOutputRecord.Get().aoDistance = inputRecord.aoDistance + si.length * (1-z);
                }

                childOutputRecord.OutputComplete();
            } else if (GetRemainingRecursionLevels() != 0) {
                // Not last level, output recursive stems (branches)

                // Compute child transform
                TreeTransformCompressedq64 childTransform;
                childTransform.SetPos(childPosition);
                childTransform.SetRot(qMul(qRotateAxisAngle(qGetZ(rotParent), parentZRotAngle),
                                           qMul(rotParent, downAngleRot)));

                // compute child length
                const float childLengthMax = params.nLength[nextLevelClamped] + random::SignedRandom(childSeed, 89) * params.nLengthV[nextLevelClamped];
                const float shapeRatio     = ShapeRatio(params.nShape[si.level], si.level == 0? ratio : (1 - 0.6 * z));
                const float childLength    = childLengthMax * (si.length * shapeRatio);

                ThreadNodeOutputRecords<GenerateTreeRecord> childOutputRecord =
                    generateTreeOutput.GetThreadNodeOutputRecords(hasChildOutput);

                // write child output record
                if (hasChildOutput) {
                    // copy constants to record
                    childOutputRecord.Get().scale = inputRecord.scale;
                    childOutputRecord.Get().seed  = childSeed;

                    // AO
                    childOutputRecord.Get().aoDistance = inputRecord.aoDistance + si.length * (1-z);

                    // Set transform an length
                    childOutputRecord.Get().trafo  = childTransform;
                    childOutputRecord.Get().length = max(0, childLength);

                    // Radius
                    const float radiusParent = GetTaperedRadius(si, params, z);
                    childOutputRecord.Get().radius = min(radiusParent * .9, inputRecord.radius * pow(childLength / si.length, params.RatioPower));

                    // Compute number of children in next level
                    if ((si.level + 2) == params.Levels) {
                        // If next level is last level (leaves), we use the leaf and blossom count
                        childOutputRecord.Get().children = int((abs(params.Leaf.Count) + abs(params.Blossom.Count)) * ShapeRatio(params.nShape[nextLevelClamped], 1.f - z)); // is instead leaves
                    } else {
                        // If next level is not last level, we use the number of branches, scaled by the child length
                        float grandchildren = params.nBranches[min(si.level + 2, params.Levels - 1)];
                        childOutputRecord.Get().children =
                            si.level == 0?
                                int(grandchildren * (0.2 + 0.8 * childLength / si.length / childLengthMax)) :
                                int(grandchildren * (1.0 - 0.5 * z));
                    }
                }

                childOutputRecord.OutputComplete();
            }

            if (childrenInStep < StemThreadGroupSize) {
                break;
            }
        }
    }
}