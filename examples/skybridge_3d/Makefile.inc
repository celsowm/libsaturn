# Skybridge's pig is the user-supplied, user-modified derivative of
# AlexEsfell's "Low-poly Pig" (CC BY 4.0). Feed the native GLB directly into
# the same generic animated-model converter used by the other examples.
# See assets/PIG_ATTRIBUTION.md for author, modification and source hash.
MODEL_GLB := examples/$(EXAMPLE)/assets/pig_source.glb
MODEL_OUT_PREFIX := $(GENERATED_DIR)/$(EXAMPLE)/pig_model
MODEL_SYMBOL := skybridge_pig
MODEL_SCALE := 4.3
MODEL_PALETTE_INDEX := 7
MODEL_SIMPLIFY := auto
MODEL_QUALITY := aggressive
MODEL_ANIMATION := Walk,Idle,Jump
MODEL_ANIMATION_FPS := 12
MODEL_MERGE_RIGID_MESHES := 1
MODEL_FACE_COLORS := on
MODEL_LIGHT_DIR := -0.5,0.6,0.8
MODEL_AMBIENT := 0.48
MODEL_DIFFUSE := 0.55
EXAMPLE_ASSETS := $(MODEL_OUT_PREFIX).c
EXAMPLE_HEADERS := $(MODEL_OUT_PREFIX).h
