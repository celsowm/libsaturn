"""The example must compile as standalone C using only public SDK headers."""
import subprocess

source = "examples/scene_cache_occlusion/main.c"
result = subprocess.run(
    ["gcc", "-std=c11", "-Wall", "-Wextra", "-Werror",
     "-Iinclude", "-I.", "-fsyntax-only", source],
    capture_output=True, text=True, check=False,
)
assert result.returncode == 0, result.stderr
print("scene_cache_occlusion public C API compilation: OK")
