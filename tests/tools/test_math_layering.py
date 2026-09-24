"""Structural gate: shared math remains a lower-level core dependency."""
from pathlib import Path

old_api = Path("src/graphics/3d/geometry/math.cpp")
old_logic = Path("src/graphics/3d/geometry/math_logic.hpp")
api = Path("src/core/math3d/api.cpp")
logic = Path("src/core/math3d/logic.hpp")
assert api.is_file() and logic.is_file()
assert not old_api.exists() and not old_logic.exists()

for path in [api, logic, Path("src/core/geometry/mesh_faces.hpp")]:
    assert path.is_file(), path
    assert "src/graphics/" not in path.read_text(), path

# Physics may consume public mesh/quad value types, not the renderer's
# private mesh-construction implementation or graphics math helpers.
for path in Path("src/physics").rglob("*"):
    if path.suffix in (".cpp", ".hpp", ".c", ".h"):
        physics_source = path.read_text()
        assert "src/graphics/" not in physics_source, path
        assert "saturn::core::mesh3d::face_quad(" not in physics_source, path

for root in ("src", "tests/host"):
    for path in Path(root).rglob("*"):
        if path.suffix not in (".cpp", ".hpp", ".c", ".h"):
            continue
        content = path.read_text()
        assert str(old_api) not in content, path
        assert str(old_logic) not in content, path

for path in (Path("src/physics/3d/world.cpp"),
             Path("src/physics/2d/collision_logic.hpp"),
             Path("src/physics/3d/collision_logic.hpp")):
    assert '#include "src/core/math3d/logic.hpp"' in path.read_text(), path

# Public physics headers must not import the VDP1 renderer transitively.
mesh_header = Path("include/saturn/mesh3d.h").read_text()
collision_header = Path("include/saturn/collide3d.h").read_text()
renderer_header = Path("include/saturn/mesh3d_draw.h").read_text()
assert '#include "saturn/render3d.h"' not in mesh_header
assert '#include "saturn/vdp1.h"' not in mesh_header
assert '#include "saturn/mesh3d_draw.h"' not in mesh_header
assert '#include "saturn/mesh3d.h"' in collision_header
assert '#include "saturn/render3d.h"' in renderer_header
assert "typedef struct sat_quad3" in Path("include/saturn/geometry3d.h").read_text()
assert "typedef struct sat_quad3" not in Path("include/saturn/render3d.h").read_text()
assert "typedef struct sat_mesh" in mesh_header
assert "typedef struct sat_mesh_draw" not in mesh_header

makefile = Path("Makefile").read_text()
assert str(old_api) not in makefile
assert str(api) in makefile
print("core math dependency contract: OK")
