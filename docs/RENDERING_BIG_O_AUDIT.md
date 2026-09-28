# Rendering Big-O Audit

**Date:** 2026-09-28  
**Scope:** runtime rendering, scene preparation, render caches, material registration, and the city frame planner. Offline import tooling and physics are listed only where they help distinguish intentional superlinear work from render-frame work.

## Executive summary

The canonical render path no longer contains a known quadratic ordering stage.

The important runtime shape is now:

```
bounded instances
    -> O(1) conservative reject when wholly outside the frustum
    -> O(V + F) preparation for surviving geometry
    -> O(F + 1024) scene-wide painter ordering
    -> O(F) bounded face dispatch/emission
```

The scene painter remains a painter's algorithm, not a Z-buffer. Intersecting polygons can still require authored subdivision; improving asymptotic ordering does not change that hardware constraint.

## Current complexity map

| Area | Current complexity | Notes |
| --- | --- | --- |
| Canonical scene ordering | **O(F + 1024)** | `paint_order_grouped_buckets`; at most 8 pass groups are tracked, so group lookup is bounded constant work. |
| Bounded instance fully outside frustum | **O(1)** with respect to V/F | Eight AABB corners plus optional VP × world matrix composition; returns before vertex projection and face validation. |
| Visible/unbounded instance preparation | **O(V + F)** | Vertices transform/project once and faces are visited once. |
| Immutable instance topology preflight | **O(1) per frame after O(F) bind** | `sat_scene3d_instance_binding_init` validates once; matching pointer/count identity avoids the separate per-frame topology scan. |
| Scene emission | **O(F)** | Near/screen clipping has fixed per-face limits, so it is bounded constant work per face. |
| Cache -> canonical scene | **O(1) cache finalization** | `sat_view_cache_finish`; the scene performs the one ordering pass later. |
| Direct cache replay ordering | **O(C log C)** | Allocation-free stable heapsort; retained for callers that require a preordered cache. |
| Low-level indexed solid mesh ordering | **O(F + 1024)** | Uses the same bucket painter instead of the old wide heapsort. |
| Public bucket paint-order | **O(N + 1024)** | `sat_paint_order_buckets8/16`; generates stable approximate painter order and destructively reuses keys as scratch. |
| Exact public index sort | **O(N log N)** | `sat_sort_indices_desc` / `sat_sort_indices16_desc`; intentionally retained when an exact key sort is explicitly requested. |
| Solid-material exact register/dedup | **O(log M)** worst case | Embedded AVL index; full construction is O(M log M), capacity <= 255. |
| Solid-material nearest-colour query | **O(M)** | Intentionally scans candidates because the query is RGB-distance nearest-neighbour, not exact-key lookup. |
| city_walk cell ordering | **O(N + 256)** | Four stable 8-bit radix passes over fixed-width dist2; source order supplies the (cz,cx) tie-break. |
| city_walk budget adjustment | **O(N * L)**, effectively O(N) | One far-to-near degradation pass and one drop pass; L = 3 fixed LOD levels. |

F = faces, V = vertices, C = cached entries, M = registered solid materials, N = planner/render items.

## Changes made by this audit

### View cache

Commit `5c2a041` removed the quadratic insertion sort from the normal scene-cache path.

- `sat_view_cache_finish()`: O(1), preserving source order for the global scene painter.
- `sat_view_cache_sort()`: O(C log C) direct-replay fallback.
- Equal-depth direct-replay entries retain append order through a bake ordinal.

This avoids sorting a cache only to feed it into another global ordering stage.

### Instance visibility

Commit `6c04586` added optional AABB bounds to `sat_scene3d_instance_t`.

A wholly off-frustum object now pays a fixed eight-corner conservative test rather than:

```
transform all V
project all V
visit/validate all F
```

This changes large scenes from doing geometry-sized work for every submitted object to geometry-sized work mainly for objects that survive the coarse visibility test.

### Immutable topology validation

Commit `4363fec` added `sat_scene3d_instance_binding_t`.

The binding validates mesh indices, material indices and material legality once in O(F). Later submissions compare the frozen topology identity in O(1). Vertex positions may still change in-place; index/material topology must remain immutable while using the binding.

### Low-level indexed mesh painter

Commit `84de316` changed `sat_draw_indexed_solid_mesh3()` from the exact wide heapsort O(F log F) to the scene's stable 1024-bucket painter O(F + 1024).

No extra per-face storage was added: the existing `depth[]` scratch is intentionally overwritten with bucket IDs and `order[]` receives the final order.

### Solid material pool

Commit `0759bbf` replaced repeated linear duplicate scans with an embedded AVL index.

- exact lookup/insert: O(log M) worst case;
- building M unique entries: O(M log M), previously O(M²);
- no heap;
- maximum metadata cost is 3 bytes × 255 nodes plus one root byte.

The nearest-colour API remains linear because it compares RGB distance against the requested candidate range.

### city_walk frame planner

Commit `0a65569` removed two quadratic patterns.

1. Cell ordering first moved off quadratic insertion sort, then to a stable fixed-width radix order: O(N + 256), preserving exact distance and (cz,cx) tie order.
2. Budget adjustment no longer restarts a reverse scan after every downgrade/drop. Because a cell can degrade at most one LOD step, one far-to-near degradation pass followed by one far-to-near drop pass preserves the policy in O(N * L), with L fixed at 3.

### Explorer projected-item painter

The follow-up migration exposes `sat_paint_order_buckets8/16()` as the public
bounded paint-order primitive and moves `infinite_explorer` from the exact
O(N log N) heapsort to O(N + 1024), preserving source order inside a bucket.

## Intentional superlinear work that remains

### Exact index sorting

`sat_sort_indices_desc()` and `sat_sort_indices16_desc()` remain O(N log N). They are explicit exact-sort utilities and do not back the canonical scene painter anymore.

`infinite_explorer` now uses `sat_paint_order_buckets8()`, so the exact
sort utilities have no known canonical-example frame-path dependency. They
remain available for callers whose keys require exact ordering.

### Vertex welding

`sat_mesh_weld_vertices()` remains O(V²) in the number of surviving vertices. It is explicitly a mesh-construction/startup operation and is documented as inappropriate for per-frame use. Current repository usage is asset/mesh preparation rather than the frame renderer.

### Physics sweep-and-prune

Sphere-pair broadphase uses insertion sort over the previous substep's order. Its worst case is quadratic, but it deliberately exploits temporal coherence and is a physics policy, not part of this rendering audit.

### Offline import/intersection tooling

Model processing may use superlinear intersection or simplification algorithms. They run offline and do not affect Saturn frame complexity.

## Constant-cost caveats

Big-O is not the whole Saturn performance story.

- The bucket painter uses 1024 `uint16_t` counters: about **2 KiB of fixed stack scratch** per ordering call.
- Projection cost contains fixed-point matrix work and divisions; reducing the number of vertices reaching projection can matter more than replacing a small-N sort.
- Near/screen clipping has bounded output counts, but each surviving clipped face can still emit more than one hardware command.
- VDP1 command capacity and overdraw remain hard limits even when CPU-side complexity is linear.

For that reason the next performance work should be driven by SH-2/Ymir profiling counters rather than replacing every bounded O(N log N) utility mechanically.

## Validation status

The focused scene/renderer GitHub Actions gate passed after these changes.

On the current audit sequence, all native host tests also passed, including:

- `test_city_walk.cpp`
- `test_render3d_indexed.cpp`
- `test_scene3d_faces_api.cpp`
- `test_scene3d_material_pool.cpp`

The broader Phase 3 workflow is currently blocked later by `tests/tools/test_example_build_lock.py`, where `make -n EXAMPLE=hello_world` exits with status 2. The same failure is present on commit `8836169`, before the indexed-mesh, material-pool and city-planner Big-O changes, so it is not evidence of a regression from this audit. Because that tool test stops the workflow, the subsequent SH-2 cross-build stages are not currently reached.

## Target runtime invariant

For the normal scene path, future changes should preserve this scaling target:

```
O(number of submitted objects)
+ O(vertices of coarse-visible objects)
+ O(faces of coarse-visible objects)
+ O(1024 fixed painter buckets)
```

Do not reintroduce per-object face sorting before the scene-wide painter, repeated immutable topology validation, or application-side object/face insertion sorts.
