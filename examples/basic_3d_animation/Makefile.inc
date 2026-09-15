# basic_3d_animation - interactive animated textured 3D model viewer.
#
# The source GLB under assets/ is an acceptance fixture owned by this
# example (see assets/LICENSE.txt); the generic tools/import_model.py
# --target saturn pipeline bakes it into a compiled animated model asset
# at build time. No per-asset logic lives in the library.
#
# If the GLB is absent, the build stops with the exact missing path
# instead of substituting another asset.
MODEL_GLB                := examples/$(EXAMPLE)/assets/male_basic_walk_30_frames_loop.glb
MODEL_OUT_PREFIX         := $(GENERATED_DIR)/$(EXAMPLE)/male_walk
MODEL_SYMBOL             := male_walk
MODEL_SCALE              := 1.0
MODEL_TEXTURE_SCALE      := 2.0
MODEL_PALETTE_INDEX      := 1
MODEL_MAX_TEXTURE_WIDTH  := 504
MODEL_MAX_TEXTURE_HEIGHT := 255
# Every face of this low-poly model samples one swatch of a color atlas, so
# it compiles to solid polygon colors with baked flat lighting instead of
# textures. That welds the UV-split vertex copies (3176 -> 792) and makes
# the full 1552-triangle mesh cheaper than a textured, simplified one.
MODEL_SIMPLIFY           := off
MODEL_FACE_COLORS        := on
MODEL_LIGHT_DIR          := -0.5,0.6,0.8
MODEL_AMBIENT            := 0.35
MODEL_DIFFUSE            := 0.75
MODEL_QUALITY            := balanced
MODEL_ANIMATION          := all
MODEL_ANIMATION_FPS      := source
EXAMPLE_ASSETS           := $(MODEL_OUT_PREFIX).c
EXAMPLE_HEADERS          := $(MODEL_OUT_PREFIX).h
