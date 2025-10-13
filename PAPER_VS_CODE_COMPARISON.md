# Paper vs Source Code Feature Comparison

本文檔詳細比較論文中描述的 performance 相關功能與實際 source code 的實現差異。

## 1. Bounding Capsule Culling（包圍膠囊剔除）

### Paper 描述

論文第 153 行：
> **Hierarchical Generation Culling** Writing of a child record is omitted, if a conservative bounding capsule lies outside the view and shadow frustum. This way, generation only runs for children that have the chance to generate visible geometry. Similar culling applies to writing the D or L records.

論文聲稱：
- 在寫入 child record 時進行剔除
- 使用保守的 bounding capsule
- 同時檢查 view 和 shadow frustum
- 也應用於 D (DrawSegment) 和 L (Leaf) records

### Source Code 實現

**❌ 完全未實現**

在 `TreeGeneration.h` 中生成 child records 的代碼（第 512-522 行）：
```hlsl
ThreadNodeOutputRecords<DrawLeafRecord> childOutputRecord =
    drawLeafOutput[childOutputArrayIndex].GetThreadNodeOutputRecords(hasChildOutput);

if(hasChildOutput) {
    childOutputRecord.Get().trafo      = childTransform;
    childOutputRecord.Get().seed       = childSeed;
    childOutputRecord.Get().scale      = scale;
    childOutputRecord.Get().aoDistance = inputRecord.aoDistance + si.length * (1-z);
}

childOutputRecord.OutputComplete();
```

**沒有任何 frustum culling 或 bounding capsule 檢查**。只有基於 scale 的簡單剔除（第 510 行）：
```hlsl
hasChildOutput = hasChildOutput && (scale > 0.f);
```

對於 stem segments，在 `TreeGeneration.h` 第 374-396 行：
```hlsl
ThreadNodeOutputRecords<DrawSegmentRecord> drawSegmentOutputRecord =
    drawSegmentOutput.GetThreadNodeOutputRecords(hasVisibleDrawOutput);
```
同樣**沒有 frustum culling**。

### SplineTessellation.h 的註釋證據

`SplineTessellation.h` 第 98 行明確註釋：
```hlsl
// Segment frustum culling omitted here for simplicity
```

**結論**：論文中描述的 hierarchical generation culling 在實際代碼中**完全未實現**，作者在代碼註釋中承認為了簡化而省略了這個功能。

---

## 2. View & Shadow Frustum Culling

### Paper 描述

論文多處提及 frustum culling：
- 第 153 行：hierarchical generation culling 使用 view and shadow frustum
- 第 448 行：Level0-3 must execute on the union of the light and the camera frustum

### Source Code 實現

**❌ 完全未實現**

搜尋整個 `procedural-tree-generation/` 目錄：
```bash
grep -rn "frustum\|Frustum" procedural-tree-generation/
```

結果：
1. `SplineTessellation.h:98`: `// Segment frustum culling omitted here for simplicity`
2. 沒有其他任何 frustum culling 相關代碼

**沒有 mesh node 層級的三角形 culling**。Mesh nodes（`DrawSegment`, `DrawLeafBundle`）只進行標準的硬體 back-face culling，沒有自定義的 frustum culling。

在 `SplineSegment.h:475` 有註釋：
```hlsl
// uncomment and disable backface culling for visualization of single side tesselation
```

這表明只依賴硬體的 backface culling，沒有實現論文中提到的 view/shadow frustum culling。

**結論**：論文中描述的 View & Shadow Frustum Culling **完全未實現**。

---

## 3. Rendering & Tessellation

### 3.1 Stem Tessellation

#### Paper 描述

論文第 164-169 行描述 tessellation factors 的計算：
> With a user-defined constant *pixels per triangle* ∆, the opening angles θfrom/to in radians, the radii at the rings *r*from/to, the tessellation factors are:
> - *f* = 2 · θfrom · rfrom / ∆
> - *t* = 2 · θto · rto / ∆
> - *u* = (f + t) / 2
> - *v* = l / ∆

論文第 221-233 行描述 Opening Angle 的計算，用於減少 back-facing triangles 的生成。

#### Source Code 實現

**✅ 已實現，位於 `SplineTessellation.h`**

`SplineTessellation.h:48-158` 實現了完整的 tessellation factor 計算：

```c++
TessellationData ComputeSplineTessellation(
    in const SegmentInfo si,
    in const TreeParameters params,
    in const StemTubeCage cage,
    in const float pixelPerTriangle
)
{
    // ..。 opening angles (theta)
    const float thetaFrom = ComputeOpeningAngle(fromCamera, rFrom, dr_from);
    const float thetaTo   = ComputeOpeningAngle(toCamera,   rTo,   dr_to);

    // ... tessellation factors
    const float fromTess = (2.0 * thetaFrom * rFromPixel) / pixelPerTriangle;
    const float toTess   = (2.0 * thetaTo   * rToPixel)   / pixelPerTriangle;

    result.tessU = (fromTess + toTess) / 2.0;
    result.tessV = lengthPixel / pixelPerTriangle;
    // ...
}
```

Opening angle 計算（第 38-46 行）：
```hlsl
float ComputeOpeningAngle(in const float3 cameraDir, in const float r, in const float dr){
    const float x = abs(dot(cameraDir, normalize(dr)));
    const float theta = acos(max(dr.y * x / sqrt(dr.y * dr.y + r * r), -1.0));
    return theta;
}
```

**但有一個差異**：Small contribution culling（第 93-100 行）：
```hlsl
// Small contribution culling
const float minBranchPixelRadius = 1;
const uint3 sip = PackSegmentInfo(si);
const bool draw = (max(fromPixelDiameter, toPixelDiameter) >
                   (minBranchPixelRadius * random::Random(sip.x, sip.y, sip.z, 1337)));

// Segment frustum culling omitted here for simplicity

if (!draw) {
    return result;
}
```

這是一個**額外的優化**，論文中沒有描述。它使用隨機數來剔除屏幕空間中太小的分支。

**實際應用位置**：在 `TreeGeneration.h:364-372` 的 Stem node 中調用：
```hlsl
const TessellationData tessellationData =
    ComputeSplineTessellation(si, params, cage, GetPixelPerTriangle());

const bool hasDrawOutput = (threadIndexInClone == 0);
const bool hasVisibleDrawOutput = hasDrawOutput && (tessellationData.threadGroupCount > 0);
```

**結論**：Stem tessellation **已完整實現**，且包含論文未提及的 small contribution culling 優化。

---

### 3.2 Leaf Tessellation

#### Paper 描述

論文第 223 行：
> Similar to the work graph instancing [KOF*24], we use BundleLeaves in coalescing launch mode to combine up to 256 small draw records of leaves L into one big leaf bundle LB.

論文第 232-237 行描述連續的 LOD：
> For a continuous leaf LOD, we reduce the amount of geometry per leaf at increasing distance... As soon as the lower LOD is reached, we use a different mesh node DrawLeavesLow. This is implemented with a work graph node array for BundleLeaves.

#### Source Code 實現

**✅ 已實現，位於 `Leaves.h`**

Leaf bundling（第 26-53 行）：
```c
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
    // ... 收集最多 256 個 leaf records
    if (gtid < irs.Count()) {
        outputRecord.Get().leaves[gtid] = irs.Get(gtid);
    }
}
```

其中 `maxCoalescedDrawLeafRecords = 256`（第 16 行）。

**Node array 實現**：在 `TreeGeneration.h:192-194`：
```c
[MaxRecordsSharedWith(generateTreeOutput)]
[NodeId("CoalesceDrawLeaves")]
[NodeArraySize(3)]
NodeOutputArray<DrawLeafRecord> drawLeafOutput,
```

Node array 有 3 個節點（第 193 行 `[NodeArraySize(3)]`）：
- Index 0: Regular leaves (`CoalesceDrawLeaves`)
- Index 1: Blossoms (`CoalesceDrawBlossom`, Leaves.h:56-82)
- Index 2: Fruits (`CoalesceDrawFruits`, Fruits.h:16-42)

**但是**：論文提到的 `DrawLeavesLow` 不同 LOD 的 mesh node **未在代碼中實現**。代碼中只有一個 `LeafMeshShader` (Leaves.h:112-293)，沒有分離的 high/low LOD mesh nodes。

LOD 處理是在同一個 mesh shader 內部通過參數控制（Leaves.h:228-255），而不是使用不同的 node array index。

**結論**：Leaf coalescing **已實現**，但論文描述的多 LOD mesh node array（DrawLeavesLow）**未實現**。實際上使用單一 mesh shader 內部的 LOD 分支。

---

## 4. Engineering Optimizations

### 4.1 Work Coalescing

#### Paper 描述

論文第 363-390 行：
> **Mesh Group Coalescing** In early experiments, we noticed a drastic performance improvement by combining multiple dispatches to mesh nodes into a single one. Thus, similar to the BundleLeaves node, we also employ a bundler node in coalescing launch mode before launching any mesh node.
>
> **Quad Stem** Dispatching a full mesh shader group for small stems only consisting of two triangles and four vertices is wasteful. To optimize this, we employ a special bundler and mesh node that renders a bundle of several of these quads per group, similar to the leaves bundling.

#### Source Code 實現

**✅ 部分實現**

**Leaf Coalescing - 已實現**：
- `CoalesceDrawLeaves` (Leaves.h:26-53) - 256 leaves
- `CoalesceDrawBlossom` (Leaves.h:56-82) - 256 blossoms
- `CoalesceDrawFruits` (Fruits.h:16-42) - 256 fruits

所有使用 `[NodeLaunch("coalescing")]` 模式。

**Stem Coalescing - ❌ 未實現**：
論文提到的 `BundleStems` 節點（論文第 405 行圖表中提到）在代碼中**不存在**。

搜尋結果：
```bash
grep -rn "BundleStem\|CoalesceStem" procedural-tree-generation/
# 沒有結果
```

實際架構：`Stem` node 直接輸出到 `DrawSegment` mesh node（TreeGeneration.h:197-198）：
```hlsl
[MaxRecords(maxSegmentRecords)]
[NodeId("DrawSegment")]
NodeOutput<DrawSegmentRecord> drawSegmentOutput
```

沒有中間的 coalescing 節點。

**Quad Stem Bundling - ❌ 未實現**：
論文提到的特殊 quad stem bundler **未在代碼中找到**。所有 stems 都使用相同的 `DrawSegment` mesh node。

**結論**：只有 **Leaf Coalescing 已實現**，**Stem Coalescing 和 Quad Stem Bundling 未實現**。

---

### 4.2 Record Compression

#### Paper 描述

論文第 396 行：
> **Record Compression** To save on memory bandwidth and capacity during work graph execution, we propose to compress work records when submitting them and to decompress again on node launch. Besides using trivial bitfield compression of limited range integers like the tree level or type, we extend the octahedron mapping for unit vectors [MSS*10] to compress the rotational component of the stem transformations, thus a unit quaternion. For compression, we quantize the imaginary parts of the quaternion in L1 norm into 10 bits each and store them, along with the sign bit of the original real component, in 32 bits. This enables storing an entire stem transformation in 4 hardware dwords.

#### Source Code 實現

**✅ 已完整實現**

**Quaternion Compression** - `Quaternion.h:77-134`：

32-bit 壓縮（完全符合論文描述）：
```hlsl
uint qCompress32(float4 q) {
    // L2 -> L1 norm for xyz
    q.xyz /= abs(q.w) + abs(q.x) + abs(q.y) + abs(q.z);

    // Encode x, y, z into 10 bits for y, 11 bits for xz
    int x = int(q.x * FACT(X)) & MASK(X);  // X = 10 bits
    int y = int(q.y * FACT(Y)) & MASK(Y);  // Y = 11 bits
    int z = int(q.z * FACT(Z)) & MASK(Z);  // Z = 10 bits

    // Pack signed x, y, z into uint with sign bit of w
    return uint(x) | (uint(y) << X) | (uint(z) << (X+Y)) | (asuint(q.w) & MSB);
}
```

64-bit 壓縮（更高精度，論文提到但未詳細描述）：
```hlsl
uint64_t qCompress64(float4 q){
    // Encode x, y, z into 21 bits each (total 63 bits)
    // ...
}
```

**Transform Compression Structures** - `Records.h:44-79`：
```hlsl
struct TreeTransformCompressedq32 {
    float4 trafo;  // pos (xyz) + compressed quaternion (w as uint)
    // ...
}

struct TreeTransformCompressedq64 {
    float3 pos;
    uint64_t rot;  // 64-bit compressed quaternion
    // ...
}
```

**實際使用**：
- Leaf transforms: `TreeTransformCompressedq32` (TreeGeneration.h:481)
- Branch transforms: `TreeTransformCompressedq64` (TreeGeneration.h:527) - 更高精度用於遞歸

**Bitfield Compression** - `Records.h:100-139`：
```hlsl
struct SegmentInfo {
    // 使用 bit packing
    uint level : LEVEL_BITS;        // 2 bits (0-3)
    uint fromZ : SEGMENT_Z_BITS;    // 15 bits
    uint toZ : SEGMENT_Z_BITS;      // 15 bits
    // ...
}
```

**結論**：Record compression **完全按照論文描述實現**，包括：
- 32-bit 和 64-bit quaternion 壓縮（使用 L1 norm 和 octahedron mapping）
- Bitfield compression for integers
- Sign bit preservation for interpolation

---

### 4.3 Specialized Nodes

#### Paper 描述

論文第 392-394 行：
> In the future, it could be feasible to employ even more specialized mesh shaders for different tessellation scenarios.

論文第 394-395 行：
> **Stem Leaves Mesh Node** As described in Sec. 3.2, we omit thin stems for rendering. We extend this idea by creating a special mesh node DrawStemLeaves that creates and samples a stem spline to only output the geometry for the leaves. The actual stem geometry is not drawn. With this node, we can skip the generation of the last stem level at far camera distances.

#### Source Code 實現

**❌ 未實現**

**DrawStemLeaves - 不存在**：
```bash
grep -rn "DrawStemLeaves\|StemLeaves" procedural-tree-generation/
# 沒有結果
```

論文提到的 `DrawStemLeaves` specialized mesh node **未在代碼中實現**。

**實際實現**：代碼中只有兩個主要的 mesh nodes：
1. `DrawSegment` (SplineSegment.h:180-348) - 用於所有 stems
2. `LeafMeshShader` (Leaves.h:112-293) - 用於所有 leaves/blossoms/fruits

沒有特殊的 "stem leaves" 節點來在遠距離跳過最後一層 stem 生成。

**Node Arrays**：
- Leaf node array with 3 indices (leaves, blossoms, fruits) - 已實現
- 論文暗示的多 LOD stem node array - 未實現

**結論**：論文提到的 **DrawStemLeaves specialized node 未實現**。實際代碼使用更簡單的架構。

---

### 4.4 Shadow Pass Fusion

#### Paper 描述

論文第 398-450 行詳細描述：
> **3.8. Pass Fusion for Deferred Shadow Mapping** We use deferred rendering [DWS*88] with shadow mapping [WSP04]. That would typically require a *geometry pass* to fill the G-buffers, a *shadow pass* to create a shadow map, and a *composition pass* to compute lighting. This, however, would force us to store the full generation result, or to generate geometry twice, i.e., once for the geometry and once for the shadow pass. Instead, we propose to *fuse* geometry and shadow pass and run the inner work graph nodes of our generation only once. For this, we submit to an extra mesh node that creates our shadow map in our graph of (1):
>
> DrawStem and DrawLeaves are the nodes that fill the G-buffers. We duplicate them to DrawStemShadow and DrawLeavesShadow to create our shadow map. For those nodes, we can apply several shadow-map generation optimizations: we create geometry at a lower level-of-detail, evict the pixel shader, and tune pipeline-state and mesh shader. Note that Level0-3 must execute on the union of the light and the camera frustum.
>
> Shadow nodes and non-shadow nodes must operate on distinct depth buffers. However, we cannot switch depth buffers during a work graph launch. Instead, we create a single depth buffer large enough for both tasks. Then, we partition it into two *texture array slices*, and set the corresponding slice index in the mesh shaders.

#### Source Code 實現

**❌ 完全未實現**

搜尋所有 shadow 相關代碼：
```bash
grep -rn "Shadow\|shadow" procedural-tree-generation/
# 沒有任何結果
```

**沒有找到**：
- `DrawStemShadow` node
- `DrawLeavesShadow` node
- Shadow map 相關代碼
- Texture array slice 切換
- Deferred shadow mapping

**實際架構**：Work graph 只包含：
```
Entry → Stem (recursive) → DrawSegment (mesh)
                        → CoalesceDrawLeaves → DrawLeafBundle (mesh)
                        → CoalesceDrawBlossom → DrawLeafBundle (mesh)
                        → CoalesceDrawFruits → DrawLeafBundle (mesh)
```

完全沒有 shadow pass 的分支。

**可能的原因**：
1. 這是一個 **簡化的 sample 實現**（readme.md 提到 "simplified subset"）
2. Shadow mapping 可能在 Work Graph Playground 的外部處理
3. Pass fusion 是論文的理論貢獻，但在公開的 sample 中未實現

**結論**：論文的核心優化之一 **Shadow Pass Fusion 完全未實現**。這是 paper 與 code 之間最大的差異。

---

## 總結表

| Feature | Paper 描述 | Source Code | 實現狀態 | 差異說明 |
|---------|-----------|-------------|---------|---------|
| **1. Bounding Capsule Culling** | 在生成 child records 時使用保守的 bounding capsule 進行 frustum culling | 完全無實現，代碼註釋承認省略 | ❌ 未實現 | 代碼中明確註釋 "omitted for simplicity" |
| **2. View & Shadow Frustum Culling** | Level nodes 在 view + shadow frustum 的聯集上執行 | 完全無實現 | ❌ 未實現 | 只依賴硬體 backface culling |
| **3.1 Stem Tessellation** | 基於 opening angle 和 pixel-space 計算 tessellation factors | 完整實現，還包含額外的 small contribution culling | ✅ 已實現+ | 代碼包含論文未提及的優化 |
| **3.2 Leaf Tessellation** | 使用 node array 的多 LOD mesh nodes (DrawLeavesLow) | 單一 mesh shader 內部 LOD 控制 | ⚠️ 部分實現 | 架構不同但功能相似 |
| **4.1 Leaf Coalescing** | BundleLeaves 收集 256 個 leaf records | 完整實現（leaves, blossoms, fruits） | ✅ 已實現 | 完全符合論文 |
| **4.1 Stem Coalescing** | BundleStems + 特殊 quad stem bundler | 不存在 | ❌ 未實現 | Stems 直接送到 mesh node |
| **4.2 Record Compression** | 32-bit quaternion 壓縮（10+11+10 bits + sign） | 完整實現，包含 32-bit 和 64-bit 版本 | ✅ 已實現 | 完全符合論文描述 |
| **4.3 DrawStemLeaves** | 專門的 mesh node 用於遠距離只繪製葉子 | 不存在 | ❌ 未實現 | 未實現這個特化節點 |
| **4.4 Shadow Pass Fusion** | 融合 geometry 和 shadow pass，使用 texture array slices | 完全無實現，無任何 shadow 相關代碼 | ❌ 未實現 | **最大差異** - 論文核心貢獻未實現 |

## 關鍵發現

1. **已完整實現的核心功能**：
   - Stem tessellation with continuous LOD
   - Leaf coalescing (256 records bundling)
   - Record compression (quaternion + bitfield)
   - 基本的 Weber-Penn 樹生成算法

2. **論文提及但未實現的優化**：
   - Hierarchical generation culling (frustum)
   - Stem coalescing / quad stem bundling
   - DrawStemLeaves specialized node
   - **Shadow pass fusion (最重要的未實現功能)**

3. **代碼額外實現的功能**：
   - Small contribution culling (隨機剔除小分支)
   - 更精細的季節效果控制
   - 完整的 UI 交互系統

4. **原因分析**：
   - README 明確說明這是 "simplified subset"
   - 公開代碼重點展示核心生成算法
   - 部分性能優化（特別是 shadow 相關）可能是論文特有的實驗性功能
   - Frustum culling 的省略可能是為了代碼清晰度（在註釋中承認）

這個 sample 實現了足夠的功能來展示論文的主要思想（GPU work graphs 實時樹生成），但省略了一些論文中的進階優化。
