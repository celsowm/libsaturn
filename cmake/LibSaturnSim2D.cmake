# The hardware-free 2D runtime modules, shipped as sources for host simulation.
#
# The installed package is SH-2 code, so a game's host tests (which run the
# game's own logic with the native compiler) cannot link libsaturn.a. These
# files are the modules that touch no hardware register, with the private
# headers they include, installed under share/libsaturn/sim/ and exported as
# the interface target LibSaturn::Sim2D. A consumer links it into a host test
# executable to compile them with its own compiler; firmware never uses it.
#
# tests/tools/test_package_sim2d.py keeps this list closed (every private
# include of a listed file is listed) and a subset of the runtime manifest.
#
# Format: only `set(NAME ...)` of plain repository-relative paths, one per line.

set(LIBSATURN_SIM2D_FILES
    src/core/math2d/api.cpp
    src/core/math2d/logic.hpp
    src/core/math2d/tables.hpp
    src/core/math3d/logic.hpp
    src/core/task/task.cpp
    src/graphics/2d/sprites/clip.cpp
    src/physics/2d/character2.cpp
    src/physics/2d/character2_logic.hpp
    src/physics/2d/collision.cpp
    src/physics/2d/collision_logic.hpp
    src/physics/2d/follow_camera2d.cpp
    src/physics/2d/grid.cpp
    src/physics/2d/grid_logic.hpp
    src/physics/2d/path2.cpp
    src/physics/2d/physics2_world.cpp
    src/physics/2d/terrain2.cpp
    src/physics/2d/terrain2_logic.hpp
    src/physics/spatial/2d.cpp
    src/physics/spatial/2d_logic.hpp
    src/physics/spatial/entity_stream2.cpp
)
