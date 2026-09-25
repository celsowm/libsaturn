#!/usr/bin/env python3
"""Guard the direction of dependencies: games may consume the library, not vice versa."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]
generic_paths = (
    root / "src/core/parallel/executor.cpp",
    root / "src/graphics/3d/scene/parallel.cpp",
    root / "src/graphics/3d/scene/faces.cpp",
    root / "src/hal/vdp1/vdp1.cpp",
)
for path in generic_paths:
    code = path.read_text(encoding="utf-8")
    assert "SAT_SKYBRIDGE" not in code, f"{path}: game-specific build flag leaked into generic library"
    assert "skybridge_3d/" not in code, f"{path}: example include leaked into generic library"
    assert "SAT_PROFILE_METRICS" in code, f"{path}: generic profiling gate missing"

faces = (root / "src/graphics/3d/scene/faces.cpp").read_text(encoding="utf-8")
assert "g_texture_registry" not in faces, "low-level painter must not own the logical texture registry"
assert "src/graphics/2d/textures/runtime.hpp" not in faces, (
    "logical texture ownership belongs to the opt-in managed scene adapter"
)
managed = (root / "src/graphics/3d/scene/managed_texture.cpp").read_text(encoding="utf-8")
assert "texture_resolve(" in managed and "validate_managed_texture(" in managed
assert "paint_order_from_keys(" not in faces, "local painter-sort implementation was reintroduced"
sorts = faces.count("paint_order_buckets(") + faces.count("paint_order_grouped_buckets(")
assert sorts == 1, "scene should sort global face queue only once"
batch = (root / "include/saturn/scene3d_faces.h").read_text(encoding="utf-8")
start = batch.index("typedef struct sat_scene3d_prepare_batch {")
end = batch.index("} sat_scene3d_prepare_batch_t;", start)
assert not re.search(r"\border\b", batch[start:end]), "batch prep regained unused sort scratch"
print("generic library/game isolation and single-painter-order: OK")
