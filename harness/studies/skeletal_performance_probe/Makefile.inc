SKELETAL_ROBOT_DEMO := 1
include examples/skeletal_animation_3d/Makefile.inc
SAT_PROFILE_METRICS := 1
MODEL_GLB := examples/skeletal_animation_3d/assets/robot_police_unit_animated.glb
MODEL_OUT_PREFIX := $(GENERATED_DIR)/skeletal_animation_3d/robot_police_skeletal
EXAMPLE_ASSETS := $(MODEL_OUT_PREFIX).c
EXAMPLE_HEADERS := $(MODEL_OUT_PREFIX).h
EXAMPLE_INCFLAGS := -I$(GENERATED_DIR)/skeletal_animation_3d -DSKELETAL_USE_PARALLEL=0
ifeq ($(SKELETAL_STUDY_COMPACT),1)
MODEL_GLB :=
EXAMPLE_ASSETS := harness/build/skeletal_compact/robot_police_skeletal.c
EXAMPLE_HEADERS := harness/build/skeletal_compact/robot_police_skeletal.h
EXAMPLE_INCFLAGS := -Iharness/build/skeletal_compact -DSKELETAL_USE_PARALLEL=0
endif
ifeq ($(SKELETAL_STUDY_DUAL),1)
EXAMPLE_INCFLAGS += -DSKELETAL_STUDY_DUAL=1
endif
