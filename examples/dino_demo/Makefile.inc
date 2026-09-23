# dino_demo animated T-Rex viewer.
#
# T-Rex source: examples/dino_demo/assets/trex_walking_seamless.glb.
# The GLB and its compiled model are attributed in assets/LICENSE.txt.
MODEL_GLB                := examples/$(EXAMPLE)/assets/trex_walking_seamless.glb
MODEL_OUT_PREFIX         := $(GENERATED_DIR)/$(EXAMPLE)/trex_model
MODEL_SYMBOL             := trex
MODEL_SCALE              := 1.0
MODEL_TEXTURE_SCALE      := 0.5
MODEL_PALETTE_INDEX      := 1
MODEL_MAX_TEXTURE_WIDTH  := 504
MODEL_MAX_TEXTURE_HEIGHT := 255
MODEL_SIMPLIFY           := 1962
MODEL_QUALITY            := balanced
MODEL_ANIMATION          := all
MODEL_ANIMATION_FPS      := 15
MODEL_FACE_COLORS        := off
MODEL_LIGHT_DIR          := -0.5,0.6,0.8
MODEL_AMBIENT            := 0.30
MODEL_DIFFUSE            := 0.85
MODEL_MAX_POSE_STREAM_BYTES := 786432
MODEL_HUD_RESERVE        := 64
EXAMPLE_ASSETS            := $(MODEL_OUT_PREFIX).c
EXAMPLE_HEADERS           := $(MODEL_OUT_PREFIX).h
