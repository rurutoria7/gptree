# TreeGeneration.h - Complete Walkthrough

## File Structure Overview

```
TreeGeneration.h
├─ Helper Functions (Lines 13-159)
│  ├─ GetStemCurve() - Calculate stem curvature
│  ├─ AddSplitSpread() - Angle adjustment for splits
│  ├─ AddVerticalAttraction() - Vertical attraction (gravity, fruit weight)
│  ├─ AddWindSway() - Wind animation
│  ├─ GetChildDownRotation() - Child node down angle
│  ├─ GetChildparentZAngle() - Child rotation around parent Z-axis
│  └─ AddFruitWeight() - Fruit gravity
│
└─ Stem Node (Lines 178-579) - Core generation node
```

---

## Part 1: Helper Functions

### 1.1 GetStemCurve() - Stem Curvature Angle
`TreeGeneration.h:13-26`

```hlsl
float GetStemCurve(...) {
    // Calculate X-axis rotation angle per segment
    float rotateX = nCurveV * random(...);  // Random variation

    if (nCurveBack == 0) {
        rotateX += nCurve / curveResolution;  // Linear curvature
    } else {
        // Reverse curvature in middle section (e.g., branch curves down then up)
        rotateX += lerp(nCurve, nCurveBack, alpha);
    }
}
```

### 1.2 AddSplitSpread() - Split Angle Adjustment
`TreeGeneration.h:29-46`

```hlsl
void AddSplitSpread(inout rot, inout splitCorrection) {
    // Calculate split angle
    // Calculate spread angle
    rot = qMul(rot, qRotateX(splitAngle));
    rot = qMul(rot, qRotateY(spreadAngle));
}
```

**Purpose**: Make split branches diverge in different directions

### 1.3 AddVerticalAttraction() - Vertical Attraction
`TreeGeneration.h:49-83`

```hlsl
void AddVerticalAttraction(inout rot) {
    attractionUp = 0;

    // 1. User-configured upward attraction
    if (level > 1) attractionUp += userConfig;

    // 2. Seasonal effects (branches droop in winter)
    attractionUp += seasonEffect;

    // 3. Fruit weight (pulls downward)
    if (hasFruit) attractionUp -= fruitWeight;

    // Apply rotation
    rot = qMul(qRotateAxisAngle(axis, curveUpSegment), rot);
}
```

### 1.4 AddWindSway() - Wind Animation
`TreeGeneration.h:101-120`

```hlsl
void AddWindSway(inout rot) {
    // Calculate wind direction
    // Calculate angle between branch and wind
    // Use Perlin noise to generate sway animation
    float forcePhase = PerlinNoise2D(position - Time * windDir);
    rot = qMul(qRotateAxisAngle(rotAxis, angle * forcePhase), rot);
}
```

*Other helper functions omitted for brevity*

---

## Part 2: Stem Node - Core Generation Logic

### 2.1 Node Declaration and I/O
`TreeGeneration.h:178-199`

```hlsl
[Shader("node")]
[NumThreads(32, 1, 1)]  // 32 threads per thread group
[NodeMaxRecursionDepth(3)]  // Max 3 recursion levels
void Stem(
    uint gtid : SV_GroupThreadId,
    DispatchNodeInputRecord<GenerateTreeRecord> ir,  // Input

    NodeOutput<GenerateTreeRecord> generateTreeOutput,      // Output: child branches
    NodeOutputArray<DrawLeafRecord> drawLeafOutput,         // Output: leaves
    NodeOutput<DrawSegmentRecord> drawSegmentOutput         // Output: render segments
)
```

**Input**: `GenerateTreeRecord` (contains position, rotation, length, radius, seed)

**Outputs**:
1. DrawSegmentRecord - sent to rendering nodes
2. GenerateTreeRecord - recursively generate next level
3. DrawLeafRecord - render leaves/fruit

---

### 2.2 Initialization
`TreeGeneration.h:201-253`

```hlsl
// Read data from input record
const GenerateTreeRecord inputRecord = ir.Get();

// Initialize segment info
SegmentInfo si;
si.level  = 3 - GetRemainingRecursionLevels();
si.length = inputRecord.length;
si.radius = inputRecord.radius;

// Calculate parameters
const float curveResolution = params.nCurveRes[si.level];  // Number of segments
const uint children = inputRecord.children;  // Number of child nodes

// Initialize transform (per-thread state)
TreeTransform trafo;
trafo.pos = inputRecord.trafo.GetPos();
trafo.rot = inputRecord.trafo.GetRot();

// Thread management variables
int cloneIndex = 0;                     // Which clone this thread belongs to
int threadIndexInClone = gtid;          // Thread index within clone
int threadCountInClone = 32;            // Number of threads owned by clone
```

---

### 2.3 Main Loop: Iterate Over Segments
`TreeGeneration.h:255-578`

```hlsl
for (int step = 0; step < curveResolution; ++step) {
    // ═══════════════════════════════════════════════════
    // Stage 1: Handle Splits (dynamic thread reallocation)
    // ═══════════════════════════════════════════════════

    // ═══════════════════════════════════════════════════
    // Stage 2: Compute Branch Transform
    // ═══════════════════════════════════════════════════

    // ═══════════════════════════════════════════════════
    // Stage 3: Dispatch DrawSegmentRecord (render current segment)
    // ═══════════════════════════════════════════════════

    // ═══════════════════════════════════════════════════
    // Stage 4: Dispatch Children (child branches or leaves)
    // ═══════════════════════════════════════════════════
}
```

Let me break down each stage in detail:

---

#### **Stage 1: Handle Splits (Dynamic Thread Reallocation)**
`TreeGeneration.h:262-303`

```hlsl
// 1.1 Calculate how many branches to split into
numSplits = round(segmentSplits + splitError);  // Error diffusion algorithm
if (step == 0) numSplits += nBaseSplits;        // First segment may have extra splits
numSplits += 1;  // +1 to include original branch

hasSplits = (numSplits > 1);

// 1.2 Reallocate threads
if (hasSplits) {
    threadsPerSplit = threadCountInClone / numSplits;

    // Example: 32 threads splitting into 4
    // Thread 0-7   → Clone 0
    // Thread 8-15  → Clone 1
    // Thread 16-23 → Clone 2
    // Thread 24-31 → Clone 3

    if (isOriginalClone) {
        threadCountInClone = threadsPerSplit + surplus;
    } else {
        threadIndexInClone = (threadIndexInClone - ...) % threadsPerSplit;
        threadCountInClone = threadsPerSplit;
    }

    // Use wave intrinsic to calculate new cloneIndex
    cloneIndex = WavePrefixCountBits(threadIndexInClone == threadCountInClone-1);
}
```

**Key Points**:
- All threads continue execution but logically belong to different clones
- Use `cloneIndex` to distinguish different clones
- Each clone computes independent transforms going forward

---

#### **Stage 2: Compute Branch Transform**
`TreeGeneration.h:309-343`

```hlsl
// 2.1 Save current transform (segment start point)
if (threadIndexInClone == 0) {
    groupClonePreTrafo[cloneIndex] = trafo;
}

// 2.2 Apply Stem Curve (base curvature)
float stemCurve = GetStemCurve(...);
trafo.rot = qMul(trafo.rot, qRotateX(stemCurve));

// 2.3 Split branches apply additional rotation
if (hasSplits && !isOriginalClone) {
    AddSplitSpread(..., trafo.rot, ...);  // Make split branches diverge
}

// 2.4 Apply vertical attraction
AddVerticalAttraction(..., trafo.rot);

// 2.5 Apply wind animation
if (!hasSplits) {
    AddWindSway(..., trafo.rot);
}

// 2.6 Move along direction
trafo.pos += qGetZ(trafo.rot) * segmentLength;

// 2.7 Save new transform (segment end point)
if (threadIndexInClone == 0) {
    groupCloneTrafo[cloneIndex] = trafo;
}
```

**Key Concepts**:
- `groupClonePreTrafo[cloneIndex]` - Segment start point
- `groupCloneTrafo[cloneIndex]` - Segment end point
- These two points form a "tube cage" for subsequent rendering

---

#### **Stage 3: Dispatch DrawSegmentRecord**
`TreeGeneration.h:345-397`

```hlsl
// Only the first thread of each clone dispatches a record
const bool threadActive = (threadIndexInClone == 0);
const bool hasDrawOutput = threadActive && (drawOutputCount < maxSegmentRecords);

if (hasDrawOutput) {
    // Calculate LOD (based on distance to camera)
    tessellationData = ComputeVisibilityAndTessellationData(
        si, params,
        groupClonePreTrafo[cloneIndex],  // Start point
        groupCloneTrafo[cloneIndex],     // End point
        pixelsPerTriangle);

    // Dispatch record
    DrawSegmentRecord record;
    record.cage.from = groupClonePreTrafo[cloneIndex];
    record.cage.to   = groupCloneTrafo[cloneIndex];
    record.si        = si;
    // ... other tessellation parameters

    drawSegmentOutput.OutputComplete();
}
```

**Purpose**: Send rendering information to DrawSegment mesh node

---

#### **Stage 4: Dispatch Children (Core Recursion Logic)**
`TreeGeneration.h:408-577`

```hlsl
// 4.1 Iterate over all children (processed in batches)
for (uint childIteration = 0; childIteration < 4; ++childIteration) {
    // Each thread processes one child
    uint stemChildIndex = childOutputCount + gtid;

    // 4.2 Calculate where child should be generated (z coordinate along branch)
    float localChildStepf = firstChildStepf + stemChildIndex * childStepfDelta;

    // 4.3 Check if this child should be generated in current step
    bool hasChildInStep = (floor(localChildStepf) == step) && ...;

    if (!hasChildInStep) continue;

    // 4.4 Calculate child transform
    // - Interpolate position on spline
    float3 childPosition = StemSpline(
        groupClonePreTrafo[childCloneIndex].pos,
        groupCloneTrafo[childCloneIndex].pos,
        t);

    // - Calculate rotation (down angle + Z-axis rotation)
    float4 childRotation = qMul(rotParent, downAngleRot);
    childRotation = qMul(qRotateAxisAngle(parentZ, parentZRotAngle), childRotation);

    // 4.5 Dispatch different records based on level
    if (si.level == params.Levels - 1) {
        // ──────────────────────────────────
        // Last level → Dispatch leaves/fruit
        // ──────────────────────────────────

        // Determine type
        bool isLeafBlossom = IsLeafBlossom(...);
        bool isFruit = isLeafBlossom && random() < Fruit.Chance;

        // Calculate scale (based on season)
        float scale = baseScale * GetSeasonLeafScale(...);

        // Apply fruit gravity
        if (isFruit) {
            AddFruitWeight(childRotation, ...);
        }

        // Dispatch DrawLeafRecord
        DrawLeafRecord record;
        record.trafo = childTransform;
        record.scale = scale;
        record.seed  = childSeed;

        drawLeafOutput[arrayIndex].OutputComplete();

    } else {
        // ──────────────────────────────────
        // Not last level → Dispatch child branch (recursion)
        // ──────────────────────────────────

        // Calculate child branch length and radius
        float childLength = childLengthMax * (si.length * shapeRatio);
        float childRadius = inputRecord.radius * pow(childLength/si.length, RatioPower);

        // Calculate number of children for next level
        if (nextLevel == lastLevel) {
            children = LeafCount + BlossomCount;
        } else {
            children = nBranches[nextLevel+1];
        }

        // Dispatch GenerateTreeRecord (triggers new Stem node)
        GenerateTreeRecord record;
        record.trafo    = childTransform;
        record.length   = childLength;
        record.radius   = childRadius;
        record.children = children;
        record.seed     = childSeed;

        generateTreeOutput.OutputComplete();  // 🔄 Recursive Stem call
    }
}
```

---

## Core Data Flow Summary

```
Entry Node
   │
   ├─ CreateTreeRecord(Level 0)
   │
   └──→ Stem Node (Level 0 - Trunk)
        │
        ├─ Main Loop: for (step = 0; step < curveResolution; ++step)
        │   │
        │   ├─ [Split] Dynamically allocate threads → multiple clones
        │   │
        │   ├─ [Transform] Calculate transform for each clone
        │   │
        │   ├─ [Output] DrawSegmentRecord → DrawSegment mesh node → Rendering
        │   │
        │   └─ [Children] At appropriate z positions:
        │       │
        │       ├─ GenerateTreeRecord → 🔄 Stem Node (Level 1)
        │       │                         │
        │       │                         └──→ ... (recursion)
        │       │
        │       └─ DrawLeafRecord → Leaf mesh node → Render leaves
        │
        └─ Loop ends
```

---

## Key Design Highlights

### 1. **Thread Reallocation Mechanism**
- Avoids dispatching numerous split records
- Uses wave intrinsics to dynamically manage threads
- Single thread group handles multiple clones

### 2. **Error Diffusion for Splits**
```hlsl
splitError -= numSplits - segmentSplits;
```
Ensures average split count matches parameters (e.g., `nSegSplits=0.5` means split every 2 segments)

### 3. **Children Uniformly Distributed Along Branch**
```hlsl
childStepfDelta = curveResolution / children;
localChildStepf = firstChildStepf + childIndex * childStepfDelta;
```

### 4. **LOD System**
Dynamically adjusts tessellation density based on distance to camera

### 5. **Shared Memory Optimization**
```hlsl
groupshared TreeTransform groupCloneTrafo[32];
```
Multiple threads share transform data for clones

---

## Split vs. Branches Comparison

| Aspect | Split (nSegSplits) | Branches (nBranches) |
|--------|-------------------|----------------------|
| **Implementation** | Thread reallocation within same node | Dispatch new GenerateTreeRecord |
| **Scope** | Within single Stem node execution | Creates new recursive Stem node |
| **Thread Management** | Subdivides existing 32 threads | New node gets fresh 32 threads |
| **Use Case** | Branch forking at same hierarchical level | Hierarchical tree structure (trunk→branches→twigs) |
| **Visual Result** | Branch splits into multiple directions | Branch grows child branches |

---

## Execution Example

### Example Tree with 3 Levels

```
Level 0 (Trunk):
├─ curveResolution = 10 segments
├─ nSegSplits = 0.5 (splits every ~2 segments)
├─ nBranches[1] = 8 (spawns 8 child branches)
│
├─ Iteration 0-1: Single clone (32 threads)
├─ Iteration 2: Split into 2 clones (16 threads each)
├─ Iteration 4: Split into 4 clones (8 threads each)
│  └─ Each clone generates DrawSegmentRecord
│
└─ At various z positions: Dispatch 8× GenerateTreeRecord → Level 1

Level 1 (Major Branches):
├─ Each branch: curveResolution = 8 segments
├─ nBranches[2] = 15 (spawns 15 twigs each)
│
└─ At various z positions: Dispatch 15× GenerateTreeRecord → Level 2

Level 2 (Twigs):
├─ Each twig: curveResolution = 6 segments
├─ children = 20 leaves
│
└─ At various z positions: Dispatch 20× DrawLeafRecord → Leaf rendering
```

**Total nodes created**: 1 + 8 + (8×15) = 129 Stem nodes, generating gigabytes of geometry

---

## Key Takeaways

1. **Stem node is the core workhorse** - handles both iterative segment generation and recursive level spawning

2. **Thread reallocation (splits) is a memory-saving optimization** - avoids work graph dispatch overhead for branches at same hierarchical level

3. **Children distribution is spatially aware** - placed along spline based on z-parameter, ensuring natural branch placement

4. **LOD is computed per-segment** - enabling continuous level-of-detail for optimal performance

5. **The entire tree generation is stateless** - each Stem invocation is independent, enabling massive parallelism on GPU

This architecture achieves **extreme geometric compression**: ~51 KiB of code generates what would be ~34.8 GiB as static meshes!
