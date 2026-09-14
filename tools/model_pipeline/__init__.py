#!/usr/bin/env python3
"""LibSaturn animated-model host pipeline (offline, host-side only).

This package compiles modern animated glTF/GLB assets into Saturn-ready
generated C/H data. Nothing here runs on the Saturn: parsing, skinning,
simplification, UV baking and reporting all happen on the host. The runtime
only sees baked vertex frames, baked face textures and compact descriptors.

Submodules keep parsing separate from Saturn compilation:

- gltf: deterministic glTF 2.0 / GLB reader (host-only representation).
- model: canonical source-model representation shared by importers.
- animation: deterministic skeletal-animation evaluator (host skinning).
- simplification: attribute-aware edge-collapse simplification backend.
- metrics: animation-aware quality metrics over sampled poses.
- silhouette: multi-view silhouette-importance pass.
- lod: validated multi-LOD derivation from the source model.
- uv_bake: canonical face-texture baking (reuses the static pipeline).
- palette: shared indexed8 palette construction (reuses static pipeline).
- saturn_profile: VDP1/VRAM/pose-memory budget profile and gates.
- emit_c: deterministic generated C/H emission (static + animation).
- report: human and machine-readable pipeline reports.
"""

from __future__ import annotations
