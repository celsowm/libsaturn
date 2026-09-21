# Transform hierarchy — initial reusable slice

This implementation introduces a caller-owned 3D transform forest in `transform3d.h`.
It extends existing `sat_model_transform3d_t` and `sat_model_transform3d_matrix`
rather than replacing the scene renderer, camera, physics, or animation systems.

## Contract

- Create stable node IDs in a bounded pool; `SAT_TRANSFORM3D_ROOT` detaches a node.
- Parent assignment rejects self-parenting and cycles. Reparenting preserves local,
  **not world**, coordinates; callers may compute a different local transform if needed.
- Mutations invalidate the hierarchy; `get_world` returns `SAT_ERR_BUSY` until
  `evaluate` succeeds. The caller owns the `uint16_t[count]` evaluation scratch.
- Evaluation is iterative and parent-first, including when the parent was created
  after the child; only changed nodes and descendants recompute their matrices.
- All matrix results use the existing 16.16 fixed-point math and T*Ry*Rx*Rz*S
  convention. Do not interpret `world` as a collision pose without checking
  scale and local-space physics requirements.
- There is no node removal, reparent-preserve-world helper, interpolation,
  scene integration, renderer ownership, or automatic synchronization with
  physics in this slice. Reset invalidates all IDs; IDs may then be reused.
- Storage must remain live for the world's lifetime. The application must not
  mutate public node fields or use the same scratch buffer as the node pool.

## Native consumers

Games can evaluate the graph once after simulation and pass a node's world
matrix to `sat_scene3d_instance_t.world`. Cameras and physics can consume
the same matrix after explicit game-side conversions. A later opt-in scene
adapter may avoid that manual binding; this initial primitive does not introduce
a parallel scene graph or a second painter.

## Validation

Run `make test` (includes `test_transform3d`) and build representative
3D examples. The added host tests cover propagation, cache reuse, cycle
rejection, capacity, detach/reset, and parents created after children.
Physical Saturn/emulator frame-time qualification remains pending.
