# Native high-level 3D runtime acceptance example.
MODEL_OBJ                := examples/basic_3d_texture/assets/sonic.obj
MODEL_OUT_PREFIX         := $(GENERATED_DIR)/$(EXAMPLE)/sonic_model
MODEL_SYMBOL             := sonic_model
MODEL_SCALE              := 1.0
MODEL_TEXTURE_SCALE      := 1.0
MODEL_PALETTE_INDEX      := 1
MODEL_MAX_TEXTURE_WIDTH  := 504
MODEL_MAX_TEXTURE_HEIGHT := 255
EXAMPLE_ASSETS           := $(MODEL_OUT_PREFIX).c
EXAMPLE_HEADERS          := $(MODEL_OUT_PREFIX).h
