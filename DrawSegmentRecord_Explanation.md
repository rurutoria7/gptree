# DrawSegmentRecord - Field Explanation

## Structure Definition
`Records.h:141-152`

```hlsl
struct DrawSegmentRecord {
    StemTubeCageCompressed cage;      // Geometry cage (start & end transforms)
    SegmentInfo  si;                  // Segment metadata
    float aoDistance;                 // Ambient occlusion distance
    float fromPoints;                 // Tessellation: points at start ring
    float toPoints;                   // Tessellation: points at end ring
    float vPoints;                    // Tessellation: rings along length
    int faceRingsPerGroup;            // Thread group subdivision
    float fromOpeningAngle;           // Culling angle at start
    float toOpeningAngle;             // Culling angle at end
    uint dispatchGrid : SV_DispatchGrid;  // Number of mesh shader thread groups
};
```

---

## Field-by-Field Explanation

### 1. `StemTubeCageCompressed cage`

**Type**: Compressed transform pair (start + end)

**Contents**:
```hlsl
struct StemTubeCageCompressed {
    TreeTransformCompressedq32 from;  // pos (float3) + rot (compressed quaternion)
    TreeTransformCompressedq32 to;    // pos (float3) + rot (compressed quaternion)
};
```

**Purpose**: Defines the spatial "cage" of the stem segment
- `from`: Transform at segment start (position + orientation)
- `to`: Transform at segment end (position + orientation)

**Usage**:
- Mesh shader interpolates between `from` and `to` to generate tubular geometry
- Forms a Bezier-like spline segment

**Visualization**:
```
    to ●──────┐  (end transform)
       │      │
       │ cage │  ← Interpolated spline
       │      │
  from ●──────┘  (start transform)
```

---

### 2. `SegmentInfo si`

**Type**: Bit-packed segment metadata

**Contents**:
```hlsl
struct SegmentInfo {
    uint level  : 2;   // Tree level (0=trunk, 1=branches, 2=twigs, 3=leaves)
    uint fromZ  : 15;  // Start position along parent branch [0-1] encoded
    uint toZ    : 15;  // End position along parent branch [0-1] encoded
    float length;      // Physical length of segment (world units)
    float radius;      // Base radius of segment (world units)
};
```

**Purpose**: Provides geometric parameters for rendering
- `level`: Determines branch thickness, texture detail
- `fromZ/toZ`: Normalized position along parent (used for tapering calculations)
- `length`: Controls texture tiling, AO calculations
- `radius`: Base radius before tapering

---

### 3. `float aoDistance`

**Purpose**: Ambient Occlusion distance parameter

**Value**: Cumulative distance from tree base to this segment's start
- Calculated as: `aoDistance = parent.aoDistance + distanceFromParent`

**Usage**:
```hlsl
// In pixel shader (SplineSegment.h:310-311)
float distance = segmentRecord.aoDistance + ((1-v) * si.length) / params.nCurveRes[si.level];
vertex.ao = fakeAOfromDistance(distance);
```

**Effect**: Inner/lower branches are darker (occluded), outer branches are lighter

**Typical Range**: 0 (trunk base) to ~50 (outer twigs)

---

### 4. `float fromPoints` & `float toPoints`

**Purpose**: Tessellation density around circumference

**Values**: Number of vertices around ring circumference
- `fromPoints`: At segment start (may be higher if thick)
- `toPoints`: At segment end (may be lower if tapered)

**Calculation** (`SplineTessellation.h:111-112`):
```hlsl
result.fromPoints = (fromOpeningAngle * fromPixelDiameter) / pixelsPerTriangle;
result.toPoints   = (toOpeningAngle   * toPixelDiameter)   / pixelsPerTriangle;
```

**Range**: 2 to ~64 vertices per ring

**Visualization**:
```
fromPoints=16        toPoints=8
     ●●●●●●●●          ●●●●
    ●        ●        ●    ●
   ●          ●      ●      ●
    ●        ●        ●    ●
     ●●●●●●●●          ●●●●
   (thick base)     (tapered tip)
```

**Adaptive**: More points when closer to camera, fewer when distant

---

### 5. `float vPoints`

**Purpose**: Tessellation density along segment length (vertical)

**Value**: Number of "rings" along the segment

**Calculation** (`SplineTessellation.h:113`):
```hlsl
result.vPoints = lengthPixel / pixelsPerTriangle;
```

**Range**: 2 to 128 rings

**Visualization**:
```
vPoints = 8

  ●●●●●●●●  ← Ring 7 (end)
  ●●●●●●●●  ← Ring 6
  ●●●●●●●●  ← Ring 5
  ●●●●●●●●  ← Ring 4
  ●●●●●●●●  ← Ring 3
  ●●●●●●●●  ← Ring 2
  ●●●●●●●●  ← Ring 1
  ●●●●●●●●  ← Ring 0 (start)
```

**Adaptive**: More rings for longer segments or when closer to camera

---

### 6. `int faceRingsPerGroup`

**Purpose**: Number of rings each mesh shader thread group handles

**Reason**: Mesh shaders have vertex/triangle limits (128 each)
- If `vPoints` is too high, split work across multiple thread groups

**Calculation** (`SplineTessellation.h:128-132`):
```hlsl
int maxRingsPerGroupV = (maxNumVerticesPerGroup  - biggestRingV) / uPointsI;
int maxRingsPerGroupT = (maxNumTrianglesPerGroup - biggestRingT) / (2 * (uPointsI - 1));
result.faceRingsPerGroup = min(vPointsI - 1, min(maxRingsPerGroupV, maxRingsPerGroupT));
```

**Example**:
- Total rings: `vPoints = 64`
- Per group: `faceRingsPerGroup = 16`
- Thread groups needed: `dispatchGrid = ceil(64/16) = 4`

**Typical Range**: 1-32 rings per group

---

### 7. `float fromOpeningAngle` & `float toOpeningAngle`

**Purpose**: Back-face culling optimization angle

**Value**: Angular extent of visible segment surface (radians)

**Concept**: Only render the part of the tube facing the camera

**Calculation** (`SplineTessellation.h:108-109`):
```hlsl
fromOpeningAngle = GetOpeningAngle(fromPos2cam, qGetZ(cageFrom.rot), fromZ, fromRingRadius_);
toOpeningAngle   = GetOpeningAngle(toPos2cam,   qGetZ(cageTo.rot),   toZ,   toRingRadius_);
```

**Visualization**:
```
        Camera
           ↓

    ╱─────────╲     ← Opening angle ~π (most of tube visible)
   │  visible  │
    ╲─────────╱


        Camera
           ↓

    ────●────●────  ← Opening angle ~π/2 (only front half visible)
        (tube seen edge-on)
```

**Range**: 0 to π radians
- π = full circumference visible (camera far away)
- π/2 = half circumference visible (typical)
- Small angle = tube nearly edge-on (optimize away back faces)

**Usage**: Determines `fromPoints` and `toPoints` (fewer points if less visible)

---

### 8. `uint dispatchGrid : SV_DispatchGrid`

**Purpose**: Number of mesh shader thread groups to launch

**Calculation** (`SplineTessellation.h:132`):
```hlsl
result.threadGroupCount = (vPointsI + result.faceRingsPerGroup - 2) / result.faceRingsPerGroup;
```

**Equivalent to**: `ceil(vPoints / faceRingsPerGroup)`

**Special Attribute**: `: SV_DispatchGrid`
- This is a Work Graphs-specific semantic
- Tells GPU to dispatch exactly this many thread groups for rendering

**Example**:
```
vPoints = 64 rings
faceRingsPerGroup = 16 rings/group
dispatchGrid = ceil(64/16) = 4 thread groups
```

**Usage in Mesh Shader** (`SplineSegment.h:175-196`):
```hlsl
[NodeMaxDispatchGrid(maxNumTriangleRingsPerSegment, 1, 1)]
void StemMeshShader(
    uint groupOfSegment : SV_GroupID,  // 0, 1, 2, 3 for dispatchGrid=4
    DispatchNodeInputRecord<DrawSegmentRecord> ir,
    ...
) {
    bool isFirstGroup = groupOfSegment == 0;
    bool isLastGroup  = groupOfSegment == (segmentRecord.dispatchGrid - 1);
    // Each group renders faceRingsPerGroup rings...
}
```

**Typical Range**: 1-128 thread groups

---

## Summary: How Fields Work Together

### Geometry Definition
```
cage (from/to transforms) + si (length/radius)
→ Defines the 3D tube shape
```

### Level-of-Detail (LOD)
```
fromPoints, toPoints, vPoints
→ Adaptive tessellation based on screen-space size
→ Closer to camera = more triangles
```

### Work Distribution
```
vPoints / faceRingsPerGroup = dispatchGrid
→ Splits large segments across multiple mesh shader invocations
```

### Rendering Optimization
```
fromOpeningAngle, toOpeningAngle
→ Only generate visible portion of tube
→ Reduces vertex count for edge-on segments
```

### Shading
```
aoDistance + si.level
→ Darker inner branches (AO)
→ Different bark textures per level
```

---

## Data Flow Example

### TreeGeneration.h → DrawSegmentRecord → Mesh Shader

```hlsl
// TreeGeneration.h:377-393 - Populate record
DrawSegmentRecord record;
record.cage.from = groupClonePreTrafo[cloneIndex];  // Start transform
record.cage.to   = groupCloneTrafo[cloneIndex];     // End transform
record.si = si;                                     // level, fromZ, toZ, length, radius
record.aoDistance = inputRecord.aoDistance + ...;   // Cumulative distance

// Compute tessellation (calls SplineTessellation.h)
tessellationData = ComputeVisibilityAndTessellationData(...);

record.fromPoints        = tessellationData.fromPoints;       // e.g., 16.5
record.toPoints          = tessellationData.toPoints;         // e.g., 12.3
record.vPoints           = tessellationData.vPoints;          // e.g., 48.7
record.faceRingsPerGroup = tessellationData.faceRingsPerGroup; // e.g., 16
record.fromOpeningAngle  = tessellationData.fromOpeningAngle;  // e.g., 2.4 rad
record.toOpeningAngle    = tessellationData.toOpeningAngle;    // e.g., 2.1 rad
record.dispatchGrid      = tessellationData.threadGroupCount;  // e.g., 3
```

```hlsl
// SplineSegment.h:173-221 - Mesh shader receives record
void StemMeshShader(
    uint gtid : SV_GroupThreadID,           // 0-127 (thread within group)
    uint groupOfSegment : SV_GroupID,       // 0, 1, 2 (which group of 3 total)
    DispatchNodeInputRecord<DrawSegmentRecord> ir,
    ...
) {
    const DrawSegmentRecord segmentRecord = ir.Get();

    // Decompress cage
    StemTubeCage cage = segmentRecord.cage.Decompress();

    // Round tessellation points to even numbers
    int fromPointsI = RoundUpMultiple2(segmentRecord.fromPoints); // 16.5 → 16
    int toPointsI   = RoundUpMultiple2(segmentRecord.toPoints);   // 12.3 → 12
    int vPointsI    = RoundUpMultiple2(segmentRecord.vPoints);    // 48.7 → 48

    // This group renders rings [groupOfSegment * faceRingsPerGroup, ...]
    int globalRingOffset = groupOfSegment * segmentRecord.faceRingsPerGroup;

    // Generate vertices along spline
    for (each vertex in this group's ring range) {
        float v = ringIndex / vPointsI;  // [0-1] along segment
        float3 pos = StemSpline(cage.from.pos, cage.to.pos, v);
        float4 rot = qSlerp(cage.from.rot, cage.to.rot, v);
        float radius = GetTaperedRadius(si, params, z);
        // ... generate vertex
    }
}
```

---

## Key Insight: Continuous LOD

All tessellation fields (`fromPoints`, `toPoints`, `vPoints`) are **computed per-frame** based on:
1. **Distance to camera** - farther = fewer triangles
2. **Screen-space size** - smaller on screen = fewer triangles
3. **Opening angle** - edge-on tubes = fewer triangles around circumference

This achieves **continuous level-of-detail** without pre-baked LOD levels!

---

## Memory Layout

Total size: ~76 bytes (uncompressed)

```
Offset  Size  Field
------  ----  -----
0       32    cage (2 × TreeTransformCompressedq32, each 16 bytes)
32      12    si (bitfield + 2 floats)
44      4     aoDistance
48      4     fromPoints
52      4     toPoints
56      4     vPoints
60      4     faceRingsPerGroup
64      4     fromOpeningAngle
68      4     toOpeningAngle
72      4     dispatchGrid
------  ----
Total:  76 bytes per segment
```

With compression (quaternions as 32-bit), this is highly memory-efficient for real-time generation!
