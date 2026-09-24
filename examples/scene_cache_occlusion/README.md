# Scene Cache Occlusion

A minimal Saturn program showing one **global far-to-near painter** for
preprojected **indexed8 textured static walls** and a dynamic RGB actor.
The actor alternates between depths in front of and behind the nearer wall.
Static walls are projected only when a camera-specific cache view is missing.

Press **A** to switch between two camera views, **B** to change the camera
distance for the current view, and **START** to exit. A camera/viewport mismatch
invalidates only its own cached view and triggers a new bake; switching back
to an unchanged view reuses its projected geometry.

No application-authored painter keys or manual VDP1 commands are involved:
the example uses `sat_view_cache_append_world` to bake native projected
corners **and their comparable linear depths once**, then uses
`sat_scene_queue_baked_view_item_material` for cached faces and
`sat_scene_submit_quad` for the moving actor. It needs no per-frame static
depth calculation and invokes exactly one `sat_scene_flush` per frame. Direct L1 drawing remains available elsewhere.

Build: `make EXAMPLE=scene_cache_occlusion all`.

**Limitations:** This is painter-order visibility, not a Z-buffer. An
intersecting polygon may still require splitting. Cached projected geometry
must have been baked for precisely the active camera; a game that animates a
cached wall must invalidate/rebake it explicitly. Texture backing remains
resident in VDP1 VRAM until the flush. The example demonstrates the runtime
path; pixel-accurate emulator or physical-hardware verification is a separate
validation gate.
