"""The example must compile as standalone C using only public SDK headers."""
import subprocess
from pathlib import Path

source = "examples/scene_cache_occlusion/main.c"
text = Path(source).read_text(encoding="utf-8")
assert "sat_view_cache_append_world(" in text
assert "sat_scene_queue_managed_camera_view(" in text
assert "sat_scene_bind_managed_textures(" in text
assert "sat_texture_create_from_surface(" in text
assert "sat_scene_depth(" not in text
assert "sat_project_quad(" not in text
result = subprocess.run(
    ["gcc", "-std=c11", "-Wall", "-Wextra", "-Werror",
     "-Iinclude", "-I.", "-fsyntax-only", source],
    capture_output=True, text=True, check=False,
)
assert result.returncode == 0, result.stderr
print("scene_cache_occlusion public C API compilation: OK")
