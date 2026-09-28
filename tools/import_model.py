#!/usr/bin/env python3
"""Public compatibility facade and executable entry point for model imports.

The implementation is split by responsibility under ``model_pipeline``;
this module keeps the historical import surface stable for tests and tools.
"""
from __future__ import annotations

from model_pipeline import animation as anim_eval
from model_pipeline import emit_c as emit_anim
from model_pipeline import face_colors as face_color_mod
from model_pipeline import gltf as gltf_mod
from model_pipeline import intersections as intersections_mod
from model_pipeline import lod as lod_mod
from model_pipeline import metrics as metrics_mod
from model_pipeline import model as srcmodel
from model_pipeline import pose_bake
from model_pipeline import quad_merge as quad_merge_mod
from model_pipeline import saturn_profile as saturn_profile_mod
from model_pipeline import silhouette as sil_mod
from model_pipeline import simplification as simp_mod
from model_pipeline.animated_import import (
    AnimatedImportResult,
    _cheapest_rotation,
    _face_command_cap,
    _glb_winding,
    _locality_order,
    _polygon_winding,
    _resolve_material_weights,
    animated_frame_times,
    import_animated_model,
    print_animated_stats,
    rate_fraction,
    select_animation_clips,
)
from model_pipeline.constants import (
    FX16_ONE,
    VDP1_COMMAND_AREA_BYTES,
    VDP1_MAX_TEXTURE_HEIGHT,
    VDP1_MAX_TEXTURE_WIDTH,
    VDP1_VRAM_BYTES,
)
from model_pipeline.errors import ImportError
from model_pipeline.import_cache import (
    IMPORT_SIGNATURE_SCHEMA,
    _atomic_write_json,
    _cache_manifest_path,
    _canonical_path,
    _import_signature,
    _incremental_cache_hit,
    _model_output_paths,
    _referenced_input_paths,
    _sha256_file,
    _write_incremental_manifest,
)
from model_pipeline.import_cli import main
from model_pipeline.obj_import import (
    ObjModel,
    _resolve_index,
    parse_mtl,
    parse_obj,
    resolve_material_texture,
)
from model_pipeline.static_import import (
    ImportResult,
    emit_c_h,
    import_model,
    print_stats,
    split_intersecting_faces,
)
from model_pipeline.texture_bake import (
    BakedFaceInput,
    _bake_face_area,
    _linear_image,
    _lut_code_palette,
    _quantize_colors,
    apply_scale,
    bake_face_rgba,
    build_shared_palette,
    canonicalize_faces,
    conform_size,
    estimate_face_size,
    float_to_fx16,
    load_rgba_image,
    map_faces_to_indices,
    quantize_face_lut4,
    uv_to_pixel,
)

if __name__ == "__main__":
    raise SystemExit(main())
