# High-speed platformer: the stage data is generated into build/generated/high_speed_platformer
# (see stage.mk); the sources next to main.c are compiled as they are.
include examples/high_speed_platformer/stage.mk

EXAMPLE_ASSETS := $(HSP_GEN_DIR)/stage.c $(HSP_GEN_DIR)/layout.c
EXAMPLE_HEADERS := $(HSP_GEN_DIR)/stage.h $(HSP_GEN_DIR)/layout.h
