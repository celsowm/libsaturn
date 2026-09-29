# ikemen_saturn - Ikemen GO screenpack assets (KFM + stage0) on Saturn.
# Real sprites/palettes converted offline by tools/ikemen_sff from the
# Ikemen-GO-Screenpack repo (see README for attribution). The runtime
# consumes only generated tables (examples/ikemen_saturn/ikemen_anim.h contract).
# Fight logic stays in examples/ikemen_saturn/ikemen_fight.c for host tests.

IKEMEN_GEN_DIR := $(GENERATED_DIR)/ikemen_saturn
KFM_FRAMES_C := $(IKEMEN_GEN_DIR)/kfm_frames.c
KFM_FRAMES_H := $(IKEMEN_GEN_DIR)/kfm_frames.h
STAGE0_PLANE_C := $(IKEMEN_GEN_DIR)/stage0_plane.c
STAGE0_PLANE_H := $(IKEMEN_GEN_DIR)/stage0_plane.h
IKEMEN_SFF_TOOL_SRCS := $(wildcard tools/ikemen_sff/*.py)

KFM_SFF := .external/Ikemen-GO-Screenpack/chars/kfm/kfm.sff
KFM_AIR := .external/Ikemen-GO-Screenpack/chars/kfm/kfm.air
STAGE0_SFF := .external/Ikemen-GO-Screenpack/stages/stage0.sff

$(KFM_FRAMES_C) $(KFM_FRAMES_H) &: $(KFM_SFF) $(KFM_AIR) $(IKEMEN_SFF_TOOL_SRCS)
	@mkdir -p $(IKEMEN_GEN_DIR)
	$(PYTHON) -m tools.ikemen_sff char \
		--sff $(KFM_SFF) --air $(KFM_AIR) \
		--actions 0,20,40,42,105,120,130,200,210,230 \
		--palettes "1,1;1,4" \
		--out-prefix $(IKEMEN_GEN_DIR)/kfm --symbol kfm

$(STAGE0_PLANE_C) $(STAGE0_PLANE_H) &: $(STAGE0_SFF) $(IKEMEN_SFF_TOOL_SRCS)
	@mkdir -p $(IKEMEN_GEN_DIR)
	$(PYTHON) -m tools.ikemen_sff stage \
		--sff $(STAGE0_SFF) \
		--layers "0,0:start=0,0:tile;0,1:start=0,185:tile" \
		--out-prefix $(IKEMEN_GEN_DIR)/stage0 --symbol stage0

EXAMPLE_ASSETS := $(KFM_FRAMES_C) $(STAGE0_PLANE_C)
