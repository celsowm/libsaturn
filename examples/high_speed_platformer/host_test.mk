# Hooks the example's simulation test (tests/host/test_high_speed_platformer.cpp) into `make test`:
# it plays the real game.c against the generated stage, with no video chip.
include examples/high_speed_platformer/stage.mk

HOST_TEST_EXTRA_test_high_speed_platformer := $(HSP_DIR)/game.c $(HSP_GEN_DIR)/stage.c $(HSP_GEN_DIR)/layout.c \
    src/physics/2d/character2.cpp src/physics/2d/terrain2.cpp src/core/math2d/api.cpp \
    src/physics/2d/physics2_world.cpp src/physics/spatial/2d.cpp src/physics/2d/collision.cpp \
    src/physics/2d/path2.cpp src/physics/2d/follow_camera2d.cpp src/physics/spatial/entity_stream2.cpp \
    src/graphics/2d/sprites/clip.cpp
$(BUILD_DIR)/tests/test_high_speed_platformer: $(HSP_STAGE_FILES) $(HSP_DIR)/game.c $(HSP_DIR)/game.h
$(BUILD_DIR)/tests/test_high_speed_platformer: HOST_CXXFLAGS += -I$(HSP_DIR)
