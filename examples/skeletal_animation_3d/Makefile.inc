ifeq ($(SKELETAL_ROBOT_DEMO),1)
# skeletal_animation_3d - rigged humanoid driven by one animated clip.
#
# Phase-8A MVP from docs/SKELETAL_ANIMATION_PLAN.md. The asset is built
# offline by tools/import_model.py --target skeletal, from the source GLB
# in assets/. The runtime path is the new
# sat_skeleton / sat_skin / sat_skel_clip_player stack; the example does
# not use the baked-pose sat_anim_* path. No per-asset logic lives in
# the library.
MODEL_GLB                := examples/$(EXAMPLE)/assets/robot_police_unit_animated.glb
MODEL_OUT_PREFIX         := $(GENERATED_DIR)/$(EXAMPLE)/robot_police_skeletal
MODEL_SYMBOL             := robot_police_skeletal
# Match the source's intrinsic rig scale; the importer leaves the model's
# per-joint rest scale untouched and only scales translation through this
# parameter. The asset is authored in metres and reads at Saturn-friendly
# size without further scaling.
MODEL_SCALE              := 1.0
MODEL_TARGET             := skeletal
# The source mesh has 28,073 vertices / 20,053 triangles / 102 joints. The
# runtime deforms every vertex every frame so the asset has to fit in
# 1 MB WRAMH alongside code, the 102-joint skeleton, the clip data and
# the working buffers. QEM-style meshopt collapse on the import side to
# 1800 triangles followed by exact skin-record sharing gives 933 vertices
# and an ~11 KB deform pool. Skeleton, skin, clip and indices occupy ~96 KB; indexed textures add ~200 KB.
# Original-surface transfer retains atlas detail even across collapsed seams.
MODEL_MAX_TRIANGLES      := 1800
MODEL_QUALITY            := aggressive
MODEL_PALETTE_INDEX      := 1
# Bake only the texel density the on-screen character can display.
MODEL_TEXEL_EXTENT       := 160
MODEL_SAMPLING           := area
MODEL_SKELETAL_SURFACE_BAKE := 1
MODEL_SKELETAL_EXPOSURE   := 3.0
MODEL_SKELETAL_STUDIO_LIGHT := 1
# ---- the only clip the asset ships with, marked as a loop so the runtime
# wraps the time advance automatically.
MODEL_LOOP_CLIPS         := 3903460652032_TempMotion
# 30 fps gives 42 samples for a 1.4 s loop; playback uses elapsed time.
MODEL_ANIMATION_FPS      := 30
EXAMPLE_ASSETS           := $(MODEL_OUT_PREFIX).c
EXAMPLE_HEADERS          := $(MODEL_OUT_PREFIX).h
EXAMPLE_GEN_DIR          := examples/$(EXAMPLE)/gen
EXAMPLE_GEN_SOURCES      := $(EXAMPLE_GEN_DIR)/robot_police_skeletal.c
# The generated C expects the header beside it; expose that include path.
EXAMPLE_INCFLAGS         := -I$(GENERATED_DIR)/$(EXAMPLE)

EXAMPLE_INCFLAGS += -DSKELETAL_ROBOT_DEMO=1
else
include examples/characters/Makefile.inc
EXAMPLE_INCFLAGS += -DCHARACTER_DEMO_MAX_ACTORS=1 -DCHARACTER_DEMO_ASSET_SLOTS=1
endif
