# hasChildInStep - Detailed Explanation

## What is `hasChildInStep`?

`hasChildInStep` is a boolean that determines **whether a specific child should be generated in the current iteration step**.

`TreeGeneration.h:421-428`

```hlsl
const bool hasChildInStep =
    // ... the child is not scaled to zero
    (stemChildScale != 0.f) &&
    // ... and the child is in the current step
    (floor(localChildStepf) == step) &&
    // ... and we have not exceeded the maximum number of children
    ((childOutputCount + gtid) < children);
```

**Translation**: "Does THIS thread need to output a child in THIS step?"

---

## The Core Problem: Spatial Distribution

Children (branches/leaves) are **distributed along the stem**, not generated all at once.

### Example Stem with 10 Children

```
Stem (10 segments, curveResolution = 10):

Step 0:  ●───────────────  Child #0
Step 1:  ─────●───────────  Child #1
Step 2:  ─────────●───────  Child #2
Step 3:  ─────────────●───  Child #3
Step 4:  ─────────────────●  Child #4
Step 5:  ●───────────────  Child #5
Step 6:  ─────●───────────  Child #6
Step 7:  ─────────●───────  Child #7
Step 8:  ─────────────●───  Child #8
Step 9:  ─────────────────●  Child #9
```

**Key Point**: Only generate children that belong to the **current step**!

---

## Breaking Down the Three Conditions

### Condition 1: `stemChildScale != 0.f`

**Purpose**: LOD culling - don't generate children that are scaled to zero

```hlsl
// TreeGeneration.h:228-231
if (si.level == (params.Levels - 1)) {
    // Reduce leaf density based on distance to camera
    ComputeChildDensityAndScale(distanceToCamera, childDensity, childScale);
}
```

**Example**:
```
Distance to camera = 100m

childDensity = 0.5  // Only generate 50% of leaves
childScale = 2.0    // But make them 2x bigger

Child #0: stemChildScale = 2.0 ✓ (generate)
Child #1: stemChildScale = 0.0 ✗ (culled, don't generate)
Child #2: stemChildScale = 2.0 ✓ (generate)
Child #3: stemChildScale = 0.0 ✗ (culled)
...
```

**Why?** Far away trees don't need every leaf - save performance by culling invisible detail

---

### Condition 2: `floor(localChildStepf) == step`

**Purpose**: Only generate children positioned at the **current segment**

#### How Children are Positioned

`TreeGeneration.h:234-236`

```hlsl
const float zoffset         = params.nBaseSize[si.level];
const float childStepfDelta = (curveResolution * (1. - zoffset)) / float(children);
const float firstChildStepf = curveResolution * zoffset + childStepfDelta * .5;
```

**Then for each child**:

`TreeGeneration.h:416`

```hlsl
const float localChildStepf = firstChildStepf + stemChildIndex * childStepfDelta;
```

**Example**: Stem with 10 segments, 20 children

```hlsl
curveResolution = 10
children = 20
zoffset = 0.0  (no base offset)

childStepfDelta = (10 * (1.0 - 0.0)) / 20 = 0.5
firstChildStepf = 10 * 0.0 + 0.5 * 0.5 = 0.25

Child #0:  localChildStepf = 0.25 + 0  * 0.5 = 0.25  → floor = 0  → Step 0 ✓
Child #1:  localChildStepf = 0.25 + 1  * 0.5 = 0.75  → floor = 0  → Step 0 ✓
Child #2:  localChildStepf = 0.25 + 2  * 0.5 = 1.25  → floor = 1  → Step 1 ✓
Child #3:  localChildStepf = 0.25 + 3  * 0.5 = 1.75  → floor = 1  → Step 1 ✓
Child #4:  localChildStepf = 0.25 + 4  * 0.5 = 2.25  → floor = 2  → Step 2 ✓
...
Child #19: localChildStepf = 0.25 + 19 * 0.5 = 9.75  → floor = 9  → Step 9 ✓
```

**Distribution**:
```
Step 0: Children 0, 1    (2 children)
Step 1: Children 2, 3    (2 children)
Step 2: Children 4, 5    (2 children)
...
Step 9: Children 18, 19  (2 children)
```

**When `step = 3`**:

```hlsl
for (each thread processing a child) {
    if (floor(localChildStepf) == 3) {
        // Only Children #6 and #7 have localChildStepf in [3.0, 4.0)
        hasChildInStep = true;
    } else {
        hasChildInStep = false;  // Wrong step, skip
    }
}
```

---

### Condition 3: `(childOutputCount + gtid) < children`

**Purpose**: Don't exceed the total number of children

**Example**: 50 total children, childIteration = 1

```hlsl
children = 50
childOutputCount = 32  // Already processed 32 children in iteration 0

Thread 0:  (32 + 0)  = 32 < 50 ✓  → Can generate Child #32
Thread 1:  (32 + 1)  = 33 < 50 ✓  → Can generate Child #33
...
Thread 17: (32 + 17) = 49 < 50 ✓  → Can generate Child #49
Thread 18: (32 + 18) = 50 < 50 ✗  → Out of bounds! Don't generate
Thread 19: (32 + 19) = 51 < 50 ✗  → Out of bounds! Don't generate
...
Thread 31: (32 + 31) = 63 < 50 ✗  → Out of bounds! Don't generate
```

**Why?** Prevents accessing invalid child indices when `children` is not a multiple of 32

---

## Complete Example Walkthrough

### Scenario
- **Stem**: 10 segments (curveResolution = 10)
- **Total children**: 20
- **Current step**: 5
- **Current childIteration**: 0
- **childOutputCount**: 0

### Step-by-Step

```hlsl
// Setup
firstChildStepf = 0.25
childStepfDelta = 0.5

// Main loop (step = 5, childIteration = 0)
for (uint childIteration = 0; childIteration < 4; ++childIteration) {
    // childIteration = 0, so childOutputCount = 0

    // Thread 0:
    stemChildIndex = 0
    localChildStepf = 0.25 + 0 * 0.5 = 0.25
    floor(0.25) = 0 ≠ 5  → hasChildInStep = false (wrong step)

    // Thread 1:
    stemChildIndex = 1
    localChildStepf = 0.25 + 1 * 0.5 = 0.75
    floor(0.75) = 0 ≠ 5  → hasChildInStep = false (wrong step)

    // ...

    // Thread 10:
    stemChildIndex = 10
    localChildStepf = 0.25 + 10 * 0.5 = 5.25
    floor(5.25) = 5 == 5  ✓
    stemChildScale = 2.0 ≠ 0  ✓
    (0 + 10) = 10 < 20  ✓
    → hasChildInStep = true  ✓ Generate Child #10!

    // Thread 11:
    stemChildIndex = 11
    localChildStepf = 0.25 + 11 * 0.5 = 5.75
    floor(5.75) = 5 == 5  ✓
    stemChildScale = 2.0 ≠ 0  ✓
    (0 + 11) = 11 < 20  ✓
    → hasChildInStep = true  ✓ Generate Child #11!

    // Thread 12:
    stemChildIndex = 12
    localChildStepf = 0.25 + 12 * 0.5 = 6.25
    floor(6.25) = 6 ≠ 5  → hasChildInStep = false (wrong step)

    // ...

    // Thread 19:
    stemChildIndex = 19
    localChildStepf = 0.25 + 19 * 0.5 = 9.75
    floor(9.75) = 9 ≠ 5  → hasChildInStep = false (wrong step)

    // Thread 20-31:
    (0 + 20) = 20 < 20  ✗
    → hasChildInStep = false (exceeded total children)
}
```

**Result**: In step 5, only threads 10 and 11 have `hasChildInStep = true`

**Output**: 2 children generated in this step (Children #10 and #11)

---

## Usage Pattern

### 1. Check if child should be generated
```hlsl
const bool hasChildInStep =
    (stemChildScale != 0.f) &&
    (floor(localChildStepf) == step) &&
    ((childOutputCount + gtid) < children);
```

### 2. Early exit optimization
```hlsl
// If no threads have children to generate, exit loop
if (WaveActiveAllTrue(!hasChildInStep)) {
    break;
}
```

### 3. Count generated children
```hlsl
const uint childrenInStep = WaveActiveCountBits(hasChildInStep);
childOutputCount += childrenInStep;
```

### 4. Allocate output record
```hlsl
ThreadNodeOutputRecords<DrawLeafRecord> childOutputRecord =
    drawLeafOutput[arrayIndex].GetThreadNodeOutputRecords(hasChildInStep ? 1 : 0);
//                                                          ^^^^^^^^^^^^^^^^^^^^
//                                                          Only request if true
```

### 5. Fill record
```hlsl
if (hasChildInStep) {
    childOutputRecord.Get().trafo = childTransform;
    childOutputRecord.Get().scale = scale;
    // ...
}
```

### 6. Complete output
```hlsl
childOutputRecord.OutputComplete();  // All threads must call this
```

---

## Visual Summary

### Without `hasChildInStep` (Wrong!)

```
Step 5 execution:

Thread 0:  Generate Child #0 at position 0.25  ✗ Wrong position!
Thread 1:  Generate Child #1 at position 0.75  ✗ Wrong position!
Thread 2:  Generate Child #2 at position 1.25  ✗ Wrong position!
...
Thread 31: Generate Child #31 at position 15.75 ✗ Out of bounds!

Result: Chaos! Children in wrong positions, duplicates, etc.
```

### With `hasChildInStep` (Correct!)

```
Step 5 execution:

Thread 0:  localChildStepf = 0.25  → floor(0.25) = 0 ≠ 5 → Skip
Thread 1:  localChildStepf = 0.75  → floor(0.75) = 0 ≠ 5 → Skip
...
Thread 10: localChildStepf = 5.25  → floor(5.25) = 5 == 5 → Generate ✓
Thread 11: localChildStepf = 5.75  → floor(5.75) = 5 == 5 → Generate ✓
Thread 12: localChildStepf = 6.25  → floor(6.25) = 6 ≠ 5 → Skip
...
Thread 31: (0 + 31) = 31 > 20 → Skip

Result: Exactly 2 children generated at correct positions in step 5!
```

---

## Key Insights

1. **Spatial Filtering**: `hasChildInStep` ensures children are generated at their correct spatial positions along the stem

2. **LOD Culling**: Skips children scaled to zero (distance-based optimization)

3. **Bounds Checking**: Prevents generating more children than specified

4. **Early Exit**: When `WaveActiveAllTrue(!hasChildInStep)`, no thread has work → exit loop early

5. **Per-Thread Decision**: Each thread independently decides whether to generate its child

---

## Common Mistake

❌ **Wrong**: Generate all children in first step
```hlsl
if (step == 0) {
    for (int i = 0; i < children; i++) {
        generate_child(i);  // All children at position 0!
    }
}
```

✓ **Correct**: Distribute children across steps using `hasChildInStep`
```hlsl
for (step = 0; step < curveResolution; ++step) {
    for (childIteration = 0; childIteration < 4; ++childIteration) {
        if (hasChildInStep) {  // Only if child belongs to this step
            generate_child(stemChildIndex);
        }
    }
}
```

**Result**: Children naturally distributed along the entire stem length!

---

## Summary

**`hasChildInStep`** is a **spatial and validity filter** that answers three questions:

1. ✓ Is this child visible? (not scaled to zero)
2. ✓ Does this child belong to the current segment? (spatial distribution)
3. ✓ Is this child within valid range? (bounds checking)

Only when **all three are true** does the thread generate the child!

This ensures **correct spatial distribution** of children along the stem while **optimizing performance** through LOD culling and early exit.
