# ChildIteration Loop - Explanation

## Problem: How to Generate 128 Children with 32 Threads?

### The Challenge

Each stem segment can have up to **128 children** (branches or leaves), but we only have **32 threads** per thread group.

**Simple (but wrong) approach**: Only generate 32 children
```hlsl
❌ Only processes first 32 children
for each thread {
    process child[gtid];  // gtid = 0-31
}
// Children 32-127 are never generated!
```

### The Solution: Batched Processing with childIteration

`TreeGeneration.h:400-406, 408-577`

```hlsl
#if USING_SOFTWARE_ADAPTER
    const uint childIterations = 32;  // 4 threads × 32 iterations = 128 children
#else
    const uint childIterations = 4;   // 32 threads × 4 iterations = 128 children
#endif

for (uint childIteration = 0; childIteration < childIterations; ++childIteration) {
    // Each thread processes one child per iteration
    uint stemChildIndex = childOutputCount + gtid;

    // ... generate child at stemChildIndex
}
```

**Result**: 32 threads × 4 iterations = **up to 128 children**

---

## How It Works: Step-by-Step

### Iteration 0: Threads 0-31 process children 0-31
```
childOutputCount = 0

Thread 0:  stemChildIndex = 0 + 0  = 0   → Child #0
Thread 1:  stemChildIndex = 0 + 1  = 1   → Child #1
Thread 2:  stemChildIndex = 0 + 2  = 2   → Child #2
...
Thread 31: stemChildIndex = 0 + 31 = 31  → Child #31

childOutputCount += 32  (if all children were in this step)
```

### Iteration 1: Threads 0-31 process children 32-63
```
childOutputCount = 32

Thread 0:  stemChildIndex = 32 + 0  = 32  → Child #32
Thread 1:  stemChildIndex = 32 + 1  = 33  → Child #33
Thread 2:  stemChildIndex = 32 + 2  = 34  → Child #34
...
Thread 31: stemChildIndex = 32 + 31 = 63  → Child #63

childOutputCount += 32
```

### Iteration 2: Threads 0-31 process children 64-95
```
childOutputCount = 64
Thread 0:  → Child #64
Thread 1:  → Child #65
...
```

### Iteration 3: Threads 0-31 process children 96-127
```
childOutputCount = 96
Thread 0:  → Child #96
Thread 1:  → Child #97
...
Thread 31: → Child #127
```

---

## Key Code Sections

### 1. Calculate Child Index
`TreeGeneration.h:414`

```hlsl
GetNthChildIndexAndScale(childOutputCount + gtid, children, childDensity, stemChildIndex, stemChildScale);
//                       ^^^^^^^^^^^^^^^^^^^^^^
//                       Batch offset + thread ID
```

**Example** (Iteration 2, Thread 5):
- `childOutputCount = 64` (children already processed)
- `gtid = 5` (thread ID)
- `stemChildIndex = 64 + 5 = 69` → Process child #69

### 2. Check if Child is in Current Step
`TreeGeneration.h:416-428`

```hlsl
// Children are distributed along the stem
const float localChildStepf = firstChildStepf + stemChildIndex * childStepfDelta;

// Only output if this child belongs to the current step
const bool hasChildInStep =
    (stemChildScale != 0.f) &&                   // Not culled by LOD
    (floor(localChildStepf) == step) &&          // In this segment
    ((childOutputCount + gtid) < children);      // Within total count
```

**Example**: If stem has 80 children distributed across 10 segments
- Step 0: Children 0-7 are generated
- Step 1: Children 8-15 are generated
- Step 5: Children 40-47 are generated
- ...

### 3. Early Exit Optimization
`TreeGeneration.h:431-434, 574-576`

```hlsl
// Exit if no threads have children in this step
if (WaveActiveAllTrue(!hasChildInStep)) {
    break;
}

// Exit if fewer children generated than threads available
if (childrenInStep < StemThreadGroupSize) {
    break;
}
```

**Why needed?**

If stem only has 50 children total:
- Iteration 0: Process children 0-31 ✓
- Iteration 1: Process children 32-49 ✓ (only 18 children, break early)
- Iteration 2-3: Skipped (early exit)

---

## Complete Example

### Scenario: 80 Children, Current Step = 3

```
Total children: 80
Children per step: ~8
Children in step 3: Children #24-31

═══════════════════════════════════════════════════════════
childIteration = 0:
─────────────────────────────────────────────────────────
childOutputCount = 0

Thread 0:  stemChildIndex = 0  → localChildStepf = 0.5  → step 0 (not step 3) → skip
Thread 1:  stemChildIndex = 1  → localChildStepf = 0.6  → step 0 (not step 3) → skip
...
Thread 24: stemChildIndex = 24 → localChildStepf = 3.2  → step 3 ✓ → Output child #24
Thread 25: stemChildIndex = 25 → localChildStepf = 3.3  → step 3 ✓ → Output child #25
Thread 26: stemChildIndex = 26 → localChildStepf = 3.4  → step 3 ✓ → Output child #26
Thread 27: stemChildIndex = 27 → localChildStepf = 3.5  → step 3 ✓ → Output child #27
Thread 28: stemChildIndex = 28 → localChildStepf = 3.6  → step 3 ✓ → Output child #28
Thread 29: stemChildIndex = 29 → localChildStepf = 3.7  → step 3 ✓ → Output child #29
Thread 30: stemChildIndex = 30 → localChildStepf = 3.8  → step 3 ✓ → Output child #30
Thread 31: stemChildIndex = 31 → localChildStepf = 3.9  → step 3 ✓ → Output child #31

childOutputCount = 0 + 8 = 8  (only 8 children output this iteration)

═══════════════════════════════════════════════════════════
childIteration = 1:
─────────────────────────────────────────────────────────
childOutputCount = 8

Thread 0:  stemChildIndex = 8 + 0  = 8   → localChildStepf = 1.0  → step 1 (not step 3) → skip
Thread 1:  stemChildIndex = 8 + 1  = 9   → localChildStepf = 1.1  → step 1 (not step 3) → skip
...
Thread 31: stemChildIndex = 8 + 31 = 39  → localChildStepf = 4.9  → step 4 (not step 3) → skip

childOutputCount = 8 + 0 = 8  (no children output this iteration)

WaveActiveAllTrue(!hasChildInStep) == true → break early
```

**Result**: 8 children output in step 3 (children #24-31)

---

## Why Not Just Launch More Threads?

### Option A: 128 Threads (Not Used)
```hlsl
[NumThreads(128, 1, 1)]  // ❌ Wasteful
void Stem(...) {
    // Each thread processes one child
    if (gtid < children) {
        process_child(gtid);
    }
}
```

**Problems**:
- Wastes threads when `children < 128`
- Reduces occupancy (fewer thread groups can run simultaneously)
- Split mechanism needs threads divisible by wave size (32)

### Option B: 32 Threads with Batching (Used)
```hlsl
[NumThreads(32, 1, 1)]  // ✓ Efficient
void Stem(...) {
    for (iteration = 0; iteration < 4; ++iteration) {
        // Each thread processes multiple children
        process_child(childOutputCount + gtid);
    }
}
```

**Benefits**:
- ✓ Better occupancy (more thread groups fit on GPU)
- ✓ Works well with split mechanism (32 threads / N clones)
- ✓ Early exit when children < 128
- ✓ Matches wave size (32) for wave intrinsics

---

## Performance Optimization: Early Exit

### Without Early Exit
```hlsl
for (childIteration = 0; childIteration < 4; ++childIteration) {
    // Always execute 4 iterations, even if only 10 children
}
// Wasted: 4 iterations × 32 threads = 128 thread-iterations
//         Only 10 needed
```

### With Early Exit
```hlsl
for (childIteration = 0; childIteration < 4; ++childIteration) {
    // ... process children

    if (WaveActiveAllTrue(!hasChildInStep)) {
        break;  // No more children in any remaining iterations
    }

    if (childrenInStep < StemThreadGroupSize) {
        break;  // Processed fewer than 32 children, no more left
    }
}
```

**Example**: If only 10 children total
- Iteration 0: Process all 10 children
- `childrenInStep = 10 < 32` → **break**
- Iterations 1-3: Skipped

**Savings**: 3 × 32 = 96 wasted thread-iterations avoided

---

## Relationship to Split Mechanism

### Scenario: 4 Clones, 80 Children

```
After split:
├─ Clone 0: Thread 0-7   (8 threads)
├─ Clone 1: Thread 8-15  (8 threads)
├─ Clone 2: Thread 16-23 (8 threads)
└─ Clone 3: Thread 24-31 (8 threads)

childIteration = 0:
├─ Thread 0:  Child #0  (on Clone 0)
├─ Thread 1:  Child #1  (on Clone 0)
├─ ...
├─ Thread 8:  Child #8  (on Clone 1)
├─ ...
├─ Thread 31: Child #31 (on Clone 3)

childIteration = 1:
├─ Thread 0:  Child #32 (on Clone 0)
├─ Thread 8:  Child #40 (on Clone 1)
├─ ...

Total: 80 children distributed across 4 clones
```

**Key Point**: Children are assigned to clones cyclically

```hlsl
const uint childCloneIndex = stemChildIndex % activeClones;
//                           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^
//  Child #0  → Clone 0
//  Child #1  → Clone 1
//  Child #2  → Clone 2
//  Child #3  → Clone 3
//  Child #4  → Clone 0
//  ...
```

This ensures **load balancing** across clones.

---

## Summary

### What childIteration Does:
1. **Enables 128 children with 32 threads** through batched processing
2. **Each iteration**: All threads process one child (offset by batch counter)
3. **Early exit**: Stops when no more children remain
4. **Distributes children**: Across multiple clones after splits

### Formula:
```
Max children = StemThreadGroupSize × childIterations
             = 32 threads × 4 iterations
             = 128 children
```

### Visual Summary:
```
Thread Group (32 threads)
     ↓
┌────────────────────────────────────┐
│ Iteration 0: Process children 0-31  │
│ Iteration 1: Process children 32-63 │
│ Iteration 2: Process children 64-95 │
│ Iteration 3: Process children 96-127│
└────────────────────────────────────┘
     ↓
Up to 128 children generated
```

This design achieves **high parallelism** (32 threads) while supporting **large child counts** (128) without wasting GPU resources!
