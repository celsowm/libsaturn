# vdp2_rbg0_debug - Debug RBG0 step by step
EXAMPLE_INPUT        := examples/vdp2_rbg0_infinite_plane/assets/seamless_sea.png
EXAMPLE_RESIZE       := 256 256
EXAMPLE_ASSET_PREFIX := $(GENERATED_DIR)/vdp2_rbg0_debug/seamless_sea
EXAMPLE_ASSETS       := $(EXAMPLE_ASSET_PREFIX).c
EXAMPLE_HEADERS      := $(EXAMPLE_ASSET_PREFIX).h
EXAMPLE_SRCS += $(EXAMPLE_DIR)/main.c
