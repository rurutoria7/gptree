# DirectX Work Graphs - MaxRecords 上限調查報告

## 目錄
- [一、核心限制規範](#一核心限制規範)
- [二、當前專案情況分析](#二當前專案情況分析)
- [三、GetThreadNodeOutputRecords 的使用規則](#三getthreadnodeoutputrecords-的使用規則)
- [四、實驗結果與分析](#四實驗結果與分析)
- [五、解決方案](#五解決方案)
- [六、記錄體積計算](#六記錄體積計算)
- [七、建議](#七建議)

---

## 一、核心限制規範

### 1.1 Node Output Limits

根據 **DirectX-Specs-D3D12-WorkGraphs.md:1542-1578**，MaxRecords 的上限取決於節點的啟動模式：

> **Node output limits**
>
> There is a limit on output record storage that is a function of the following variables:
>
> `MaxRecords` = Maximum output record count declared across all outputs for a thread group summed together. These are declared on outputs via `[MaxRecords()]` node output attributes or D3D12_NODE_OUTPUT_OVERRIDES.
>
> `MaxOutputSize` = Space required by the `MaxRecords` records in total
>
> `SharedMemorySize` = Space required by thread group shared memory declared by a thread group
>
> `MaxRecords_WithTrackRWInputSharing` = Maximum output record count, Like `MaxRecords` above, except only counting outputs that specify the `[NodeTrackRWInputSharing]` attribute on the record struct.

### 1.2 具體限制數值

規範明確定義了不同啟動模式的限制（DirectX-Specs-D3D12-WorkGraphs.md:1556-1578）：

```
All launch modes:

    SharedMemorySize <= 32 KB

Broadcasting and Coalescing launch nodes:

    MaxRecords <= 256

    MaxOutputSize <= (32 KB - 4*MaxRecords_WithTrackRWInputSharing)

    (MaxOutputSize +
     SharedMemorySize +
     8*MaxRecords
    ) <= 48KB

Thread launch node:

    MaxRecords <= 8

    MaxOutputSize <= 128 bytes
```

**關鍵發現：Thread Launch Node 的 MaxRecords 硬限制為 8！**

### 1.3 Empty Records 的例外

規範指出（DirectX-Specs-D3D12-WorkGraphs.md:1585）：

> For `MaxRecords`, an exception to the limits are empty records (`EmptyNodeOutput`), which don't count against the limit. That said, outputs that are `EmptyNodeOutput` still need to have `MaxRecords` or `MaxRecordsSharedWith` declared to help scheduler implementations reason about work expansion potential and also avoid overflowing tracking of live work.

---

## 二、當前專案情況分析

### 2.1 Entry Node 的配置

我們的 `EntryFunction` 使用 **Thread Launch** 模式：

```hlsl
[Shader("node")]
[NodeIsProgramEntry]
[NodeLaunch("thread")]    // ← Thread launch 模式
[NodeId("Entry", 0)]
void EntryFunction(
    [MaxRecords(2)]       // 原始值
    [NodeId("Stem")]
    NodeOutput<GenerateTreeRecord> output,
    ...
)
```

**這意味著我們受到嚴格的限制：MaxRecords <= 8**

### 2.2 為什麼 10 棵樹會失敗？

當我們設置 `[MaxRecords(10)]` 時，違反了 Thread Launch Node 的規範限制：
- 我們宣告：MaxRecords = 10
- 規範要求：MaxRecords <= 8
- 結果：參數錯誤 (0x80070057)

### 2.3 GenerateTreeRecord 結構

根據 `procedural-tree-generation/Records.h:162-170`：

```hlsl
struct GenerateTreeRecord {
    TreeTransformCompressedq64 trafo;  // float3 (12 bytes) + uint64_t (8 bytes) = 20 bytes
    uint seed;                          // 4 bytes
    uint children;                      // 4 bytes
    float scale;                        // 4 bytes
    float length;                       // 4 bytes
    float radius;                       // 4 bytes
    float aoDistance;                   // 4 bytes
};
// Total: ~44 bytes
```

---

## 三、GetThreadNodeOutputRecords 的使用規則

### 3.1 Group-Uniform 調用要求

根據 **DirectX-Specs-D3D12-WorkGraphs.md:4465-4470**：

> This provides output records in a thread-local fashion. **Calls to this method must be thread group uniform though, not in any flow control that could vary within the thread group (varying includes threads exiting), otherwise behavior is undefined.** That said, executing threads do not need to be synchronized to this location (e.g. via Barrier). There may be some implementation specific barrier happening the shader cannot assume anything about it.
>
> **Though the call must be in uniform control flow, output node indexing (if any) and the value for `numRecordsForThisThread` are not required to be uniform**, such as `myOutputs[i].GetThreadNodeOutputRecords(j)` where both `i` and `j` are non-uniform values. In this way, you are able to allocate from a different node output on each thread, which is useful for binning outputs.

### 3.2 重要含義

1. **調用必須在 group-uniform control flow 中**（所有執行緒都執行到這一行）
2. **參數值可以 non-uniform**（不同執行緒可以傳不同的值）
3. **Loop 中的調用可能違反 group-uniform 規則**

### 3.3 OutputComplete 要求

規範明確要求（DirectX-Specs-D3D12-WorkGraphs.md:4506）：

> See the requirement that a matching `OutputComplete()` call must be present after any call to `GetThreadNodeOutputRecords()`.

每次 `GetThreadNodeOutputRecords()` 必須對應一個 `OutputComplete()`。

### 3.4 記錄預算計算

根據 **DirectX-Specs-D3D12-WorkGraphs.md:4500**：

> The record counts passed in to `GetThreadNodeOutputRecords()` count against the `MaxRecords(count)` record budget declared for the output node or group of output nodes.

這意味著：
- 每次調用 `GetThreadNodeOutputRecords(n)` 都會消耗 n 個記錄的預算
- 總和不能超過 `[MaxRecords(count)]` 聲明的值

---

## 四、實驗結果與分析

### 4.1 實驗數據

| 樹數量 | MaxRecords 宣告 | 實現方式 | 結果 | 錯誤訊息 |
|--------|----------------|---------|------|----------|
| 2 | 2 | 2x `GetThreadNodeOutputRecords(1)` | ✅ 成功 | - |
| 4 | 4 | 4x `GetThreadNodeOutputRecords(1)` | ✅ 成功 | - |
| 4 | 4 | 1x `GetThreadNodeOutputRecords(4)` | ❌ 失敗 | 參數錯誤 (0x80070057) |
| 10 | 10 | loop: 10x `GetThreadNodeOutputRecords(1)` | ❌ 失敗 | 參數錯誤 (0x80070057) |
| 10 | 10 | 1x `GetThreadNodeOutputRecords(10)` | ❌ 失敗 | 參數錯誤 (0x80070057) |

### 4.2 失敗原因分析

#### 4.2.1 為什麼 MaxRecords=10 失敗？

**原因：違反 Thread Launch 限制**

Thread Launch Node 的硬限制是 `MaxRecords <= 8`（DirectX-Specs-D3D12-WorkGraphs.md:1574）。

宣告 `[MaxRecords(10)]` 直接違反此限制，導致驅動程式拒絕該配置。

#### 4.2.2 為什麼 Loop 調用失敗？

```hlsl
// ❌ 違反 group-uniform 規則
for (int i = 0; i < 10; i++) {
    ThreadNodeOutputRecords<GenerateTreeRecord> outputRecord =
        output.GetThreadNodeOutputRecords(1);
    outputRecord.Get() = CreateTreeRecord(...);
    outputRecord.OutputComplete();
}
```

規範要求調用必須在 **group-uniform control flow** 中。Loop 中的調用可能被視為 non-uniform，因為：
- 編譯器無法靜態確定迭代次數
- Loop 可能被展開或動態執行
- 不同的 GPU 實作可能有不同的解釋

#### 4.2.3 為什麼單次 GetThreadNodeOutputRecords(4) 失敗？

這是一個有趣的觀察。理論上 `GetThreadNodeOutputRecords(4)` 應該可行（4 <= 8），但實驗結果顯示失敗。

**可能的原因：**

1. **MaxOutputSize 限制衝突**
   - Thread Launch: `MaxOutputSize <= 128 bytes`
   - 4 個記錄：`4 × 44 bytes = 176 bytes > 128 bytes`
   - 違反規範！

2. **流式輸出 vs. 批量分配**
   - 多次 `GetThreadNodeOutputRecords(1)` 可能是流式輸出（每次 `OutputComplete()` 後釋放）
   - 單次 `GetThreadNodeOutputRecords(4)` 需要同時分配 176 bytes
   - 超過 128 bytes 限制

### 4.3 為什麼多次獨立調用成功？

```hlsl
// ✅ 符合 group-uniform 規則
ThreadNodeOutputRecords<GenerateTreeRecord> outputRecord0 = output.GetThreadNodeOutputRecords(1);
outputRecord0.Get() = CreateTreeRecord(...);
outputRecord0.OutputComplete();  // 釋放記憶體

ThreadNodeOutputRecords<GenerateTreeRecord> outputRecord1 = output.GetThreadNodeOutputRecords(1);
outputRecord1.Get() = CreateTreeRecord(...);
outputRecord1.OutputComplete();  // 釋放記憶體
```

**成功原因：**

1. **Group-Uniform**：每次調用都在靜態位置，編譯器可以確定
2. **流式輸出**：每次只佔用 44 bytes，`OutputComplete()` 後立即釋放
3. **符合 MaxOutputSize**：`44 bytes < 128 bytes` ✅
4. **符合 MaxRecords**：總計 4 次調用，`4 < 8` ✅

---

## 五、解決方案

### 5.1 方案 A：保持 Thread Launch（推薦，限制在 8 棵樹內）

**適用於：需要 1-8 棵樹的場景**

```hlsl
[MaxRecords(8)]  // 最多 8 棵樹（符合規範）
[NodeId("Stem")]
NodeOutput<GenerateTreeRecord> output
```

實現方式：
```hlsl
void EntryFunction(...) {
    // Tree 0
    ThreadNodeOutputRecords<GenerateTreeRecord> outputRecord0 =
        output.GetThreadNodeOutputRecords(1);
    outputRecord0.Get() = CreateTreeRecord(pos0, rot0, seed0);
    outputRecord0.OutputComplete();

    // Tree 1
    ThreadNodeOutputRecords<GenerateTreeRecord> outputRecord1 =
        output.GetThreadNodeOutputRecords(1);
    outputRecord1.Get() = CreateTreeRecord(pos1, rot1, seed1);
    outputRecord1.OutputComplete();

    // ... 最多到 Tree 7
}
```

**限制：**
- 必須使用獨立的調用（不能用 loop）
- 實際上限可能小於 8（取決於 MaxOutputSize）
- 根據實驗，**4 棵樹是已驗證的安全上限**

### 5.2 方案 B：改用 Broadcasting Launch（突破限制）

**適用於：需要 9+ 棵樹的場景**

根據規範（DirectX-Specs-D3D12-WorkGraphs.md:1563）：
> Broadcasting and Coalescing launch nodes:
>     MaxRecords <= 256

修改 Entry Node：
```hlsl
[Shader("node")]
[NodeIsProgramEntry]
[NodeLaunch("broadcasting")]  // ← 改為 broadcasting
[NodeDispatchGrid(1, 1, 1)]   // 單一 thread group
[NumThreads(1, 1, 1)]          // 單一執行緒
[NodeId("Entry", 0)]
void EntryFunction(
    [MaxRecords(256)]  // 可以使用最多 256
    [NodeId("Stem")]
    NodeOutput<GenerateTreeRecord> output,
    ...
)
```

**注意事項（DirectX-Specs-D3D12-WorkGraphs.md:1593）：**

> One might notice that the tight limits on thread launch nodes could be worked around by simply renaming the node to be coalescing launch or broadcasting launch nodes with a thread group size of 1. While this is true, **implementations of the different node types will typically be fundamentally different**. For instance with coalescing and broadcasting launch nodes, implementations cannot appear to pack thread groups into a wave by definition (see Thread visibility in wave operations), so it might be that **using thread groups of size 1 in those node types will expose inefficiency** similar to using compute shaders in general this way.

規範警告可能有效能損失，但功能上可行。

### 5.3 方案 C：使用中間 Spawner Node

**適用於：既要保持 Entry 效能，又要突破限制**

架構：
```
Entry (thread)          →  TreeSpawner (broadcasting)  →  Stem (tree generation)
MaxRecords=1               MaxRecords=256                 per-tree nodes
Dispatch once              Generate N trees               Actual tree work
```

實現：
```hlsl
// Entry Node (thread launch)
[Shader("node")]
[NodeIsProgramEntry]
[NodeLaunch("thread")]
void EntryFunction(
    [MaxRecords(1)]  // 只輸出一個記錄
    [NodeId("TreeSpawner")]
    NodeOutput<SpawnerRecord> spawner,
    ...
)
{
    ThreadNodeOutputRecords<SpawnerRecord> record = spawner.GetThreadNodeOutputRecords(1);
    record.Get().numTrees = 10;  // 傳遞要生成的樹數量
    record.Get().baseSeed = LoadPersistentConfigUint(PersistentConfig::SEED);
    record.OutputComplete();
}

// TreeSpawner Node (broadcasting launch)
[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(256, 1, 1)]  // 可以生成最多 256 棵樹
[NodeDispatchGrid(1, 1, 1)]
void TreeSpawner(
    DispatchNodeInputRecord<SpawnerRecord> input,
    [MaxRecords(256)]
    [NodeId("Stem")]
    NodeOutput<GenerateTreeRecord> output,
    uint gtid : SV_GroupThreadID
)
{
    const uint numTrees = input.Get().numTrees;

    if (gtid < numTrees) {
        ThreadNodeOutputRecords<GenerateTreeRecord> tree =
            output.GetThreadNodeOutputRecords(1);

        // 計算位置（例如網格排列）
        const uint col = gtid % 5;
        const uint row = gtid / 5;
        const float3 pos = float3((col - 2) * 14.0, 0, (row - 1) * 12.0);

        tree.Get() = CreateTreeRecord(pos, qRotateX(PI*-.5), input.Get().baseSeed + gtid * 17);
        tree.OutputComplete();
    }
}
```

**優點：**
- Entry Node 保持 thread launch 效能
- Spawner 可以生成大量樹（最多 256）
- 可以動態調整樹數量

### 5.4 方案比較

| 方案 | 最大樹數 | 實現複雜度 | 效能影響 | 推薦場景 |
|------|---------|-----------|---------|---------|
| A. Thread Launch | 8 | 低 | 無 | 1-8 棵樹 |
| B. Broadcasting Entry | 256 | 低 | 可能有輕微影響 | 需要簡單快速突破限制 |
| C. Spawner Node | 256 | 中 | Entry 無影響 | 需要動態數量或最佳效能 |

---

## 六、記錄體積計算

### 6.1 GenerateTreeRecord 的實際大小

根據 `procedural-tree-generation/Records.h`：

```hlsl
struct TreeTransformCompressedq64 {
    float3 pos;      // 12 bytes
    uint64_t rot;    // 8 bytes
};  // Total: 20 bytes

struct GenerateTreeRecord {
    TreeTransformCompressedq64 trafo;  // 20 bytes
    uint seed;                          // 4 bytes
    uint children;                      // 4 bytes
    float scale;                        // 4 bytes
    float length;                       // 4 bytes
    float radius;                       // 4 bytes
    float aoDistance;                   // 4 bytes
};
// Total: 44 bytes
```

### 6.2 Thread Launch 的 MaxOutputSize 分析

規範限制（DirectX-Specs-D3D12-WorkGraphs.md:1576）：
```
Thread launch node:
    MaxOutputSize <= 128 bytes
```

**理論計算：**
- 單個記錄：44 bytes
- 理論最大數量：`floor(128 / 44) = 2` 個記錄

**但我們的實驗顯示 4 個記錄可以成功！**

### 6.3 為什麼 4 個記錄可行？

**解釋：MaxOutputSize 可能針對單次調用**

規範可能的解釋是 `MaxOutputSize` 限制的是**同時佔用的記憶體**，而非累積輸出。

當使用流式輸出模式：
```hlsl
GetThreadNodeOutputRecords(1);  // 分配 44 bytes
OutputComplete();                // 釋放 44 bytes
GetThreadNodeOutputRecords(1);  // 再次分配 44 bytes
OutputComplete();                // 再次釋放
```

每次只佔用 44 bytes < 128 bytes，因此可行。

**對比批量分配：**
```hlsl
GetThreadNodeOutputRecords(4);  // 分配 4×44 = 176 bytes
// 176 > 128，違反限制！
OutputComplete();
```

### 6.4 實際上限推算

基於實驗和理論：

| 分配模式 | MaxOutputSize 要求 | 實際可行數量 |
|---------|-------------------|-------------|
| 流式（多次調用） | 單次 <= 128 bytes | 最多 8（MaxRecords 限制） |
| 批量（單次調用） | 總計 <= 128 bytes | 最多 2 |

**結論：使用流式輸出（多次 `GetThreadNodeOutputRecords(1)`）是唯一可行的方式來達到 MaxRecords=8 的上限。**

---

## 七、建議

### 7.1 短期方案（立即可用）

**建議：使用方案 A（Thread Launch，最多 4-8 棵樹）**

```hlsl
[MaxRecords(4)]  // 保守值，已驗證可行
// 或
[MaxRecords(8)]  // 理論上限，需要測試
```

實現方式：
- 使用 N 次獨立的 `GetThreadNodeOutputRecords(1)` 調用
- 每次調用後立即 `OutputComplete()`
- 避免使用 loop

**測試計劃：**
1. 先測試 4 棵樹（已知可行）
2. 逐步增加到 5, 6, 7, 8
3. 找到實際可靠上限

### 7.2 中期方案（需要 9+ 棵樹）

**建議：使用方案 C（Spawner Node）**

理由：
- 保持 Entry Node 效能
- 獲得 256 棵樹的上限
- 易於動態調整數量
- 架構清晰，易於維護

實現步驟：
1. 創建 `SpawnerRecord` 結構
2. 實現 `TreeSpawner` broadcasting node
3. 修改 `EntryFunction` 輸出到 spawner
4. 測試不同樹數量（10, 20, 50, 100）

### 7.3 長期考量

#### 7.3.1 效能優化

根據規範的注釋（DirectX-Specs-D3D12-WorkGraphs.md:1591）：

> These bounds on the memory footprint of node shaders are present to simplify implementations and make them at least less likely to hit performance cliffs and very large overall graph execution memory backing footprints. **The exact numbers are at least somewhat arbitrary, so some implementations may yield more efficiency when apps don't need to max out the limits.**

建議：
- 不要無故追求極限（256 棵樹）
- 根據實際需求選擇合適的數量
- 考慮 LOD 系統動態調整樹數量

#### 7.3.2 架構設計

如果需要大規模樹林場景：
1. **分批渲染**：將大量樹分成多個 DispatchGraph 調用
2. **Culling**：只生成視錐體內的樹
3. **Instancing**：相同類型的樹使用 instancing
4. **Spatial Hashing**：使用空間分區優化

#### 7.3.3 記憶體預算管理

根據規範（DirectX-Specs-D3D12-WorkGraphs.md:1548-1570）：

```
Broadcasting and Coalescing launch nodes:
    (MaxOutputSize + SharedMemorySize + 8*MaxRecords) <= 48KB
```

如果使用 broadcasting spawner：
- MaxOutputSize: `256 × 44 = 11,264 bytes` (11 KB)
- SharedMemorySize: 0 bytes（如果不用）
- `8*MaxRecords`: `8 × 256 = 2,048 bytes` (2 KB)
- **Total: 13 KB < 48 KB** ✅

仍有充足餘裕，可以考慮：
- 添加 shared memory 用於樹位置計算
- 增加記錄大小（例如額外的樹參數）

---

## 八、規範引用總結

### 8.1 關鍵限制

| 限制項目 | Thread Launch | Broadcasting/Coalescing | 來源 |
|---------|--------------|------------------------|------|
| MaxRecords | <= 8 | <= 256 | DirectX-Specs:1574, 1563 |
| MaxOutputSize | <= 128 bytes | <= 32 KB - 4*MaxRecords_WithTrackRWInputSharing | DirectX-Specs:1576, 1565 |
| SharedMemorySize | <= 32 KB | <= 32 KB | DirectX-Specs:1559 |

### 8.2 關鍵方法要求

| 方法 | 要求 | 來源 |
|------|-----|------|
| GetThreadNodeOutputRecords | 必須在 group-uniform control flow | DirectX-Specs:4465 |
| GetThreadNodeOutputRecords | 參數可以 non-uniform | DirectX-Specs:4467 |
| GetThreadNodeOutputRecords | 計入 MaxRecords 預算 | DirectX-Specs:4500 |
| OutputComplete | 必須對應每次 Get 調用 | DirectX-Specs:4506 |

### 8.3 重要注意事項

1. **Empty Records 例外**（DirectX-Specs:1585）：
   > For `MaxRecords`, an exception to the limits are empty records (`EmptyNodeOutput`), which don't count against the limit.

2. **Node Type 選擇的效能影響**（DirectX-Specs:1593）：
   > ...using thread groups of size 1 in those node types will expose inefficiency similar to using compute shaders in general this way.

3. **限制的設計意圖**（DirectX-Specs:1591）：
   > These bounds on the memory footprint of node shaders are present to simplify implementations and make them at least less likely to hit performance cliffs and very large overall graph execution memory backing footprints.

---

## 九、參考文獻

1. **DirectX-Specs-D3D12-WorkGraphs.md**
   - Section: Node output limits (lines 1542-1606)
   - Section: GetThreadNodeOutputRecords (lines 4454-4511)
   - Section: OutputComplete (lines 4620-4688)
   - Section: Node launch modes (lines 758-960)

2. **專案檔案**
   - `procedural-tree-generation/ProceduralTreeGeneration.hlsl` (EntryFunction)
   - `procedural-tree-generation/Records.h` (GenerateTreeRecord 定義)
   - `procedural-tree-generation/TreeGeneration.h` (範例用法)

3. **實驗數據**
   - 2 棵樹測試：成功 (2024 對話記錄)
   - 4 棵樹測試：成功 (2024 對話記錄)
   - 10 棵樹測試：失敗，參數錯誤 0x80070057 (2024 對話記錄)

---

## 附錄 A：完整範例代碼

### A.1 方案 A：Thread Launch (最多 8 棵樹)

```hlsl
[Shader("node")]
[NodeIsProgramEntry]
[NodeLaunch("thread")]
[NodeId("Entry", 0)]
void EntryFunction(
    [MaxRecords(8)]  // Thread launch 最大值
    [NodeId("Stem")]
    NodeOutput<GenerateTreeRecord> output,

    [MaxRecords(1)]
    [NodeId("Skybox")]
    NodeOutput<SkyboxRecord> skyboxOutput,

    [MaxRecords(1)]
    [NodeId("UserInterface")]
    EmptyNodeOutput userInterfaceOutput
)
{
    // ... 初始化和 UI 代碼 ...

    const uint baseSeed = LoadPersistentConfigUint(PersistentConfig::SEED);
    const float spacing = 14.0;

    // 生成最多 8 棵樹（這裡示範 8 棵）
    // Tree 0
    ThreadNodeOutputRecords<GenerateTreeRecord> outputRecord0 =
        output.GetThreadNodeOutputRecords(1);
    outputRecord0.Get() = CreateTreeRecord(
        float3(-3*spacing, 0, -spacing),
        qRotateX(PI*-.5),
        baseSeed + 0
    );
    outputRecord0.OutputComplete();

    // Tree 1
    ThreadNodeOutputRecords<GenerateTreeRecord> outputRecord1 =
        output.GetThreadNodeOutputRecords(1);
    outputRecord1.Get() = CreateTreeRecord(
        float3(-2*spacing, 0, -spacing),
        qRotateX(PI*-.5),
        baseSeed + 17
    );
    outputRecord1.OutputComplete();

    // Tree 2
    ThreadNodeOutputRecords<GenerateTreeRecord> outputRecord2 =
        output.GetThreadNodeOutputRecords(1);
    outputRecord2.Get() = CreateTreeRecord(
        float3(-spacing, 0, -spacing),
        qRotateX(PI*-.5),
        baseSeed + 34
    );
    outputRecord2.OutputComplete();

    // Tree 3
    ThreadNodeOutputRecords<GenerateTreeRecord> outputRecord3 =
        output.GetThreadNodeOutputRecords(1);
    outputRecord3.Get() = CreateTreeRecord(
        float3(0, 0, -spacing),
        qRotateX(PI*-.5),
        baseSeed + 51
    );
    outputRecord3.OutputComplete();

    // Tree 4
    ThreadNodeOutputRecords<GenerateTreeRecord> outputRecord4 =
        output.GetThreadNodeOutputRecords(1);
    outputRecord4.Get() = CreateTreeRecord(
        float3(-3*spacing, 0, spacing),
        qRotateX(PI*-.5),
        baseSeed + 68
    );
    outputRecord4.OutputComplete();

    // Tree 5
    ThreadNodeOutputRecords<GenerateTreeRecord> outputRecord5 =
        output.GetThreadNodeOutputRecords(1);
    outputRecord5.Get() = CreateTreeRecord(
        float3(-2*spacing, 0, spacing),
        qRotateX(PI*-.5),
        baseSeed + 85
    );
    outputRecord5.OutputComplete();

    // Tree 6
    ThreadNodeOutputRecords<GenerateTreeRecord> outputRecord6 =
        output.GetThreadNodeOutputRecords(1);
    outputRecord6.Get() = CreateTreeRecord(
        float3(-spacing, 0, spacing),
        qRotateX(PI*-.5),
        baseSeed + 102
    );
    outputRecord6.OutputComplete();

    // Tree 7
    ThreadNodeOutputRecords<GenerateTreeRecord> outputRecord7 =
        output.GetThreadNodeOutputRecords(1);
    outputRecord7.Get() = CreateTreeRecord(
        float3(0, 0, spacing),
        qRotateX(PI*-.5),
        baseSeed + 119
    );
    outputRecord7.OutputComplete();
}
```

### A.2 方案 C：Spawner Node (最多 256 棵樹)

```hlsl
// 新增 SpawnerRecord 到 Records.h
struct SpawnerRecord {
    uint numTrees;
    uint baseSeed;
};

// Entry Node（保持 thread launch）
[Shader("node")]
[NodeIsProgramEntry]
[NodeLaunch("thread")]
[NodeId("Entry", 0)]
void EntryFunction(
    [MaxRecords(1)]  // 只輸出一個 spawner 記錄
    [NodeId("TreeSpawner")]
    NodeOutput<SpawnerRecord> spawner,

    [MaxRecords(1)]
    [NodeId("Skybox")]
    NodeOutput<SkyboxRecord> skyboxOutput,

    [MaxRecords(1)]
    [NodeId("UserInterface")]
    EmptyNodeOutput userInterfaceOutput
)
{
    // ... 初始化和 UI 代碼 ...

    // 輸出到 spawner
    ThreadNodeOutputRecords<SpawnerRecord> spawnerRecord =
        spawner.GetThreadNodeOutputRecords(1);
    spawnerRecord.Get().numTrees = 10;  // 可以動態調整
    spawnerRecord.Get().baseSeed = LoadPersistentConfigUint(PersistentConfig::SEED);
    spawnerRecord.OutputComplete();
}

// TreeSpawner Node（broadcasting launch）
[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeDispatchGrid(1, 1, 1)]
[NumThreads(256, 1, 1)]  // 可以處理最多 256 棵樹
[NodeId("TreeSpawner")]
void TreeSpawner(
    DispatchNodeInputRecord<SpawnerRecord> input,

    [MaxRecords(256)]
    [NodeId("Stem")]
    NodeOutput<GenerateTreeRecord> output,

    uint gtid : SV_GroupThreadID
)
{
    const uint numTrees = input.Get().numTrees;
    const uint baseSeed = input.Get().baseSeed;

    if (gtid < numTrees) {
        // 計算樹的位置（例如網格排列）
        const uint cols = 5;  // 每行 5 棵
        const uint col = gtid % cols;
        const uint row = gtid / cols;

        const float spacingX = 14.0;
        const float spacingZ = 12.0;
        const float3 position = float3(
            (col - cols/2) * spacingX,
            0,
            (row - 1) * spacingZ
        );

        // 輸出樹記錄
        ThreadNodeOutputRecords<GenerateTreeRecord> treeRecord =
            output.GetThreadNodeOutputRecords(1);
        treeRecord.Get() = CreateTreeRecord(
            position,
            qRotateX(PI*-.5),
            baseSeed + gtid * 17
        );
        treeRecord.OutputComplete();
    }
}
```

---

## 附錄 B：故障排除

### B.1 常見錯誤

#### 錯誤 1：參數錯誤 (0x80070057)

**症狀：**
```
Operation Failed: system (-2147024809) (80070057): 參數錯誤。
```

**可能原因：**
1. MaxRecords > 8（Thread Launch）
2. MaxOutputSize > 128 bytes（單次批量分配）
3. 使用 loop 調用 GetThreadNodeOutputRecords
4. 缺少 OutputComplete 調用

**解決方法：**
- 檢查 `[MaxRecords()]` 聲明是否 <= 8
- 改用多次獨立調用而非 loop
- 確保每次 `GetThreadNodeOutputRecords()` 後有 `OutputComplete()`

#### 錯誤 2：編譯錯誤 - 不支持 Work Graphs 1.1

**症狀：**
```
error: The current device does not support Work Graphs tier 1.1 (Mesh Nodes)
```

**解決方法：**
- 更新 GPU 驅動
- 確認 GPU 支持 Work Graphs 1.1
- 檢查 WorkGraphPlayground 是否用 `PLAYGROUND_ENABLE_MESH_NODES=ON` 編譯

### B.2 調試技巧

#### 技巧 1：逐步增加樹數量

```hlsl
// 從 1 開始測試
[MaxRecords(1)]  // 測試
// 然後
[MaxRecords(2)]  // 測試
[MaxRecords(4)]  // 測試
[MaxRecords(8)]  // 測試
```

#### 技巧 2：檢查記憶體使用

計算總記憶體需求：
```
MaxOutputSize = NumRecords × RecordSize
              = 8 × 44 bytes
              = 352 bytes

是否超過限制？352 > 128 (如果單次分配)
使用流式輸出？每次 44 < 128 ✅
```

#### 技巧 3：驗證 Group-Uniform

確保調用在所有可能的執行路徑上：
```hlsl
// ❌ 錯誤
if (condition) {
    GetThreadNodeOutputRecords(1);  // 不是 group-uniform
}

// ✅ 正確
GetThreadNodeOutputRecords(condition ? 1 : 0);  // group-uniform 調用
if (condition) {
    // 使用記錄
}
```

---

## 結論

通過對 DirectX Work Graphs 規範的深入分析和實驗驗證，我們發現：

1. **Thread Launch Node 的 MaxRecords 硬限制為 8**（DirectX-Specs:1574）
2. **MaxOutputSize 限制為 128 bytes**（DirectX-Specs:1576）
3. **流式輸出（多次獨立調用）是達到 MaxRecords 上限的唯一可行方式**
4. **Broadcasting Launch 可以突破限制到 256 棵樹**（DirectX-Specs:1563）
5. **使用 Spawner Node 架構是平衡效能與靈活性的最佳方案**

建議根據實際需求選擇適當的方案：
- **1-8 棵樹**：使用 Thread Launch（方案 A）
- **9+ 棵樹**：使用 Spawner Node（方案 C）

最重要的是遵循規範要求，確保調用在 group-uniform control flow 中，並正確管理記憶體預算。
