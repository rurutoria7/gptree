1. Bounding Capsule 的剔除是在哪裏進行的？生成樹葉的時候？生成 clone 的時候？進到下一個 record 的時候？
2. Triangle-Level Culling
	- 也就是，在 Mesh Node 内部的 Culling
	- Paper 说了什么？
	- 程式码实现了什么？
3. Rendering & Tessellation
	3.1 Stem Tessellation:
		- [v] Tessellation factors
		- [v] Open Angle (back-facing triangles)
	3.2 Leaves Tessellation:
        - For a continuous leaf LOD, we reduce the amount of geometry per leaf at increasing distance... As soon as the lower LOD is reached, we use a different mesh node DrawLeavesLow. This is implemented with a work graph node array for BundleLeaves.
        - [ ] Per Leaf Reduce #geometry
        - [ ] DrawLeavesLow Mesh Node
            - (node array)
- 4. Engineering Optimization
	- 4.1 Work Coalescing
        - 4.1.1 **Mesh Group Coalescing** In early experiments, we noticed a drastic performance improvement by combining multiple dispatches to mesh nodes into a single one. Thus, similar to the BundleLeaves node, we also employ a bundler node in coalescing launch mode before launching any mesh node.
            - [v] Leaves Coalescing
            - [ ] Stem Coalescing
        - 4.1.2 **Quad Stem** Dispatching a full mesh shader group for small stems only consisting of two triangles and four vertices is wasteful. To optimize this, we employ a special bundler and mesh node that renders a bundle of several of these quads per group, similar to the leaves bundling.
            - [ ] All Stem use "DrawSegment" Mesh Node
	- 4.2 [v] Record Compression
        - Record Compression To save on memory bandwidth and capacity during work graph execution, we propose to compress work records when submitting them and to decompress again on node launch. Besides using trivial bitfield compression of limited range integers like the tree level or type, we extend the octahedron mapping for unit vectors [MSS*10] to compress the rotational component of the stem transformations, thus a unit quaternion. For compression, we quantize the imaginary parts of the quaternion in L1 norm into 10 bits each and store them, along with the sign bit of the original real component, in 32 bits. This enables storing an entire stem transformation in 4 hardware dwords.
	- 4.3 [ ] Specialized Nodes
        - **Stem Leaves Mesh Node** As described in Sec. 3.2, we omit thin stems for rendering. We extend this idea by creating a special mesh node DrawStemLeaves that creates and samples a stem spline to only output the geometry for the leaves. The actual stem geometry is not drawn. With this node, we can skip the generation of the last stem level at far camera distances.
        - **Node Arrays**：
            - Leaf node array with 3 indices (leaves, blossoms, fruits) - 已實現
	- 4.4 [ ] Shadow Pass Fusion
        > **3.8. Pass Fusion for Deferred Shadow Mapping** We use deferred rendering [DWS*88] with shadow mapping [WSP04]. That would typically require a *geometry pass* to fill the G-buffers, a *shadow pass* to create a shadow map, and a *composition pass* to compute lighting. This, however, would force us to store the full generation result, or to generate geometry twice, i.e., once for the geometry and once for the shadow pass. Instead, we propose to *fuse* geometry and shadow pass and run the inner work graph nodes of our generation only once. For this, we submit to an extra mesh node that creates our shadow map in our graph of (1):
        >
        > DrawStem and DrawLeaves are the nodes that fill the G-buffers. We duplicate them to DrawStemShadow and DrawLeavesShadow to create our shadow map. For those nodes, we can apply several shadow-map generation optimizations: we create geometry at a lower level-of-detail, evict the pixel shader, and tune pipeline-state and mesh shader. Note that Level0-3 must execute on the union of the light and the camera frustum.
        >
        > Shadow nodes and non-shadow nodes must operate on distinct depth buffers. However, we cannot switch depth buffers during a work graph launch. Instead, we create a single depth buffer large enough for both tasks. Then, we partition it into two *texture array slices*, and set the corresponding slice index in the mesh shaders.

1. Record-Level Culling
    - [ ] Frustrum Culling
2. Mesh Node (Drawing & Tessellation)
	- 3.1 Stem Tessellation:
		- [ ] Tessellation factors
		- [ ] Open Angle (back-facing triangles)
	- 3.2 Leaves Tessellation:
        - For a continuous leaf LOD, we reduce the amount of geometry per leaf at increasing distance... As soon as the lower LOD is reached, we use a different mesh node DrawLeavesLow. This is implemented with a work graph node array for BundleLeaves.
        - [ ] Per Leaf Reduce #geometry
        - [ ] DrawLeavesLow Mesh Node
            - (node array)
3. Advanced Optimization
	- 3.1 Work Coalescing
        - 3.1.1 [ ] **Mesh Group Coalescing**
            - only Leaves
        - 3.1.2 [ ] **Quad Stem**
	- 3.2 [x] Record Compression
	- 3.3 [ ] Specialized Nodes
	- 3.4 [ ] Shadow Pass Fusion