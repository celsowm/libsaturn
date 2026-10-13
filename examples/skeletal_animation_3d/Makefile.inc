# skeletal_animation_3d - rigged humanoid driven by one animated clip.
#
# Phase-8A MVP from docs/SKELETAL_ANIMATION_PLAN.md. The asset is built
# offline by tools/import_model.py --target skeletal (committed in gen/
# alongside the source GLB in assets/). The runtime path is the new
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
# The source mesh has 28,073 vertices / 6,684 triangles / 102 joints. The
# runtime deforms every vertex every frame so the asset has to fit in
# 1 MB WRAMH alongside code, the 102-joint skeleton, the clip data and
# the working buffers. QEM-style meshopt collapse on the import side to
# 1000 triangles gives a 152 KB ROM plus an 865-vertex deform pool
# (~10 KB), well within Saturn's budget while preserving the
# silhouette convincingly. The aggressive quality preset protects
# joint-anchored vertices so the rig still hangs together.
MODEL_MAX_TRIANGLES      := 1000
MODEL_QUALITY            := aggressive
# ---- the only clip the asset ships with, marked as a loop so the runtime
# wraps the time advance automatically.
MODEL_LOOP_CLIPS         := 3903460652032_TempMotion
# 60 fps keeps the clip quantisation small (84 frames for a 1.4 s loop).
MODEL_ANIMATION_FPS      := 60
EXAMPLE_ASSETS           := $(MODEL_OUT_PREFIX).c
EXAMPLE_HEADERS          := $(MODEL_OUT_PREFIX).h
EXAMPLE_GEN_DIR          := examples/$(EXAMPLE)/gen
EXAMPLE_GEN_SOURCES      := $(EXAMPLE_GEN_DIR)/robot_police_skeletal.c
# The generated C expects the header beside it; expose that include path.
EXAMPLE_INCFLAGS         := -I$(GENERATED_DIR)/$(EXAMPLE)