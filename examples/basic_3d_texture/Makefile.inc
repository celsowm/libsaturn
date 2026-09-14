# basic_3d_texture - interactive textured 3D model viewer.
#
# The source OBJ/MTL/PNG under assets/ are import fixtures owned by this
# example; the generic tools/import_model.py bakes them into a compiled
# model asset at build time. No Sonic-specific logic lives in the library.
MODEL_OBJ                := examples/$(EXAMPLE)/assets/sonic.obj
MODEL_OUT_PREFIX         := $(GENERATED_DIR)/$(EXAMPLE)/sonic_model
MODEL_SYMBOL             := sonic_model
MODEL_SCALE              := 1.0
MODEL_TEXTURE_SCALE      := 1.0
MODEL_PALETTE_INDEX      := 1
MODEL_MAX_TEXTURE_WIDTH  := 504
MODEL_MAX_TEXTURE_HEIGHT := 255
EXAMPLE_ASSETS           := $(MODEL_OUT_PREFIX).c
EXAMPLE_HEADERS          := $(MODEL_OUT_PREFIX).h
