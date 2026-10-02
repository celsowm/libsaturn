# LibSaturn runtime source manifest.
#
# This file is the single authoritative inventory of the reusable runtime.
# CMake consumes it directly; the Makefile reads the same lists through
# tools/library_sources.py. Adding a runtime source means editing this file
# and nothing else (tests/tools/test_library_source_manifest.py fails when
# the manifest and src/ disagree).
#
# Format contract (parsed by tools/library_sources.py): only `set(NAME ...)`
# blocks of plain repository-relative paths, one per line, `#` comments.

set(LIBSATURN_CORE_SOURCES
    src/core/app/api.cpp
    src/core/app/core.cpp
    src/core/geometry/mesh_api.cpp
    src/core/math2d/api.cpp
    src/core/math3d/api.cpp
    src/core/memory/api.cpp
    src/core/parallel/api.cpp
    src/core/parallel/executor.cpp
    src/core/runtime/dma.cpp
    src/core/runtime/format.cpp
    src/core/runtime/irq.cpp
    src/core/runtime/panic.cpp
    src/core/runtime/scu_dsp.cpp
    src/core/runtime/smpc.cpp
    src/core/runtime/state.cpp
    src/core/runtime/time.cpp
    src/core/startup/early_init.c
    src/core/startup/newlib_stubs.c
    src/core/startup/slave_init.cpp
)

set(LIBSATURN_GRAPHICS_SOURCES
    src/graphics/2d/font/api.cpp
    src/graphics/2d/font/text.cpp
    src/graphics/2d/hud.cpp
    src/graphics/2d/palette/api.cpp
    src/graphics/2d/palette/registry.cpp
    src/graphics/2d/rendering/api.cpp
    src/graphics/2d/rendering/runtime.cpp
    src/graphics/2d/screen.cpp
    src/graphics/2d/sprites/animation.cpp
    src/graphics/2d/surfaces/api.cpp
    src/graphics/2d/textures/api.cpp
    src/graphics/2d/textures/runtime.cpp
    src/graphics/3d/animation/api.cpp
    src/graphics/3d/camera/follow.cpp
    src/graphics/3d/camera/orbit.cpp
    src/graphics/3d/geometry/mesh.cpp
    src/graphics/3d/geometry/model.cpp
    src/graphics/3d/geometry/model_upload.cpp
    src/graphics/3d/geometry/transform.cpp
    src/graphics/3d/materials/pool.cpp
    src/graphics/3d/rendering/api.cpp
    src/graphics/3d/rendering/fade.cpp
    src/graphics/3d/rendering/indexed.cpp
    src/graphics/3d/rendering/surface.cpp
    src/graphics/3d/scene/api.cpp
    src/graphics/3d/scene/faces.cpp
    src/graphics/3d/scene/managed_texture.cpp
    src/graphics/3d/scene/parallel.cpp
    src/graphics/3d/scene/scene.cpp
    src/graphics/3d/scene/transform.cpp
    src/graphics/3d/scene/view_cache.cpp
    src/graphics/3d/scene/view_cache_world.cpp
    src/graphics/vdp1/api.cpp
    src/graphics/vdp1/color_calc.cpp
    src/graphics/vdp2/api.cpp
    src/graphics/vdp2/bitmap.cpp
    src/graphics/vdp2/color_calc.cpp
    src/graphics/vdp2/color_offset.cpp
    src/graphics/vdp2/compose.cpp
    src/graphics/vdp2/environment.cpp
    src/graphics/vdp2/layers.cpp
    src/graphics/video.cpp
)

set(LIBSATURN_AUDIO_SOURCES
    src/audio/driver/api.cpp
    src/audio/effects/api.cpp
    src/audio/playback/api.cpp
    src/audio/playback/music.cpp
    src/audio/playback/sound.cpp
    src/audio/playback/state.cpp
    src/audio/playback/voice.cpp
    src/audio/streaming/api.cpp
    src/audio/streaming/runtime.cpp
    src/audio/synthesis/api.cpp
)

set(LIBSATURN_HAL_SOURCES
    src/hal/cd/block.cpp
    src/hal/cd/mpeg.cpp
    src/hal/comm/uart.cpp
    src/hal/dual_sh2/api.cpp
    src/hal/dual_sh2/lifecycle.cpp
    src/hal/dual_sh2/mailbox.cpp
    src/hal/dual_sh2/memory.cpp
    src/hal/dual_sh2/signal.cpp
    src/hal/scsp/driver.cpp
    src/hal/scsp/dsp.cpp
    src/hal/scsp/scsp.cpp
    src/hal/scu/dma.cpp
    src/hal/scu/dsp.cpp
    src/hal/scu/irq.cpp
    src/hal/scu/scu.cpp
    src/hal/sh2/cache.cpp
    src/hal/sh2/cpu.cpp
    src/hal/sh2/dmac.cpp
    src/hal/sh2/frt.cpp
    src/hal/sh2/interrupt.cpp
    src/hal/smpc/smpc.cpp
    src/hal/storage/abus.cpp
    src/hal/storage/backup.cpp
    src/hal/storage/ram_cart.cpp
    src/hal/vdp1/vdp1.cpp
    src/hal/vdp2/color_calc.cpp
    src/hal/vdp2/vdp2.cpp
)

set(LIBSATURN_INPUT_SOURCES
    src/input/api.cpp
    src/input/runtime.cpp
)

set(LIBSATURN_PHYSICS_SOURCES
    src/physics/2d/api.cpp
    src/physics/2d/collision.cpp
    src/physics/2d/grid.cpp
    src/physics/2d/terrain2.cpp
    src/physics/2d/character2.cpp
    src/physics/2d/physics2_world.cpp
    src/physics/2d/path2.cpp
    src/physics/2d/follow_camera2d.cpp
    src/physics/3d/collision.cpp
    src/physics/3d/sweep.cpp
    src/physics/3d/sweep_full.cpp
    src/physics/3d/transform.cpp
    src/physics/3d/world.cpp
    src/physics/spatial/2d.cpp
    src/physics/spatial/3d.cpp
    src/physics/spatial/voxel_terrain.cpp
)

set(LIBSATURN_RESOURCES_SOURCES
    src/resources/assets.cpp
    src/resources/plan.cpp
)

set(LIBSATURN_STORAGE_SOURCES
    src/storage/cartridge/abus.cpp
    src/storage/cartridge/api.cpp
    src/storage/cartridge/vfs.cpp
    src/storage/cd/api.cpp
    src/storage/cd/filesystem.cpp
    src/storage/files/api.cpp
    src/storage/files/asset_runtime.cpp
    src/storage/save/api.cpp
    src/storage/save/schema.cpp
)

# Startup assembly. Linked as loose objects by the Makefile and as the
# whole-archive LibSaturn::Startup target by CMake; never part of Core.
set(LIBSATURN_STARTUP_SOURCES
    src/core/startup/crt0.s
    src/core/startup/ip_stub.s
    src/core/startup/slave_entry.s
)
