"""Structural gate: shared math remains a lower-level core dependency."""
from pathlib import Path

old_api = Path("src/graphics/3d/geometry/math.cpp")
old_logic = Path("src/graphics/3d/geometry/math_logic.hpp")
api = Path("src/core/math3d/api.cpp")
logic = Path("src/core/math3d/logic.hpp")
assert api.is_file() and logic.is_file()
assert not old_api.exists() and not old_logic.exists()

for path in [api, logic]:
    assert "src/graphics/" not in path.read_text(), path

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

makefile = Path("Makefile").read_text()
assert str(old_api) not in makefile
assert str(api) in makefile
print("core math dependency contract: OK")
