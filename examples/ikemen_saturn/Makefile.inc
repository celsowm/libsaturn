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
KFM_SOUNDS_C := $(IKEMEN_GEN_DIR)/kfm_sounds.c
KFM_SOUNDS_H := $(IKEMEN_GEN_DIR)/kfm_sounds.h
IKEMEN_SFF_TOOL_SRCS := $(wildcard tools/ikemen_sff/*.py)
IKEMEN_SND_TOOL := tools/ikemen_snd.py

KFM_SFF := .external/Ikemen-GO-Screenpack/chars/kfm/kfm.sff
KFM_AIR := .external/Ikemen-GO-Screenpack/chars/kfm/kfm.air
KFM_SND := .external/Ikemen-GO-Screenpack/chars/kfm/kfm.snd
COMMON_SND := .external/Ikemen-GO-Screenpack/data/common.snd
STAGE0_SFF := .external/Ikemen-GO-Screenpack/stages/stage0.sff

$(KFM_FRAMES_C) $(KFM_FRAMES_H) &: $(KFM_SFF) $(KFM_AIR) $(IKEMEN_SFF_TOOL_SRCS)
	@mkdir -p $(IKEMEN_GEN_DIR)
	$(PYTHON) -m tools.ikemen_sff char \
		--sff $(KFM_SFF) --air $(KFM_AIR) \
		--actions 0,11,20,41,105,120,130,200,210,230 \
		--palettes "1,1;1,4" \
		--out-prefix $(IKEMEN_GEN_DIR)/kfm --symbol kfm

$(STAGE0_PLANE_C) $(STAGE0_PLANE_H) &: $(STAGE0_SFF) $(IKEMEN_SFF_TOOL_SRCS)
	@mkdir -p $(IKEMEN_GEN_DIR)
	$(PYTHON) -m tools.ikemen_sff stage \
		--sff $(STAGE0_SFF) \
		--layers "0,0:start=0,0:tile;0,1:start=0,185:tile" \
		--out-prefix $(IKEMEN_GEN_DIR)/stage0 --symbol stage0

$(KFM_SOUNDS_C) $(KFM_SOUNDS_H) &: $(KFM_SND) $(COMMON_SND) $(IKEMEN_SND_TOOL)
	@mkdir -p $(IKEMEN_GEN_DIR)
	$(PYTHON) $(IKEMEN_SND_TOOL) \
		--out-prefix $(IKEMEN_GEN_DIR)/kfm_sounds --symbol kfm_sounds \
		--sound punch_whiff $(KFM_SND) 0 0 \
		--sound kick_whiff $(KFM_SND) 0 1 \
		--sound punch_hit $(COMMON_SND) 5 0 \
		--sound kick_hit $(COMMON_SND) 5 1

EXAMPLE_ASSETS := $(KFM_FRAMES_C) $(STAGE0_PLANE_C) $(KFM_SOUNDS_C)
EXAMPLE_HEADERS += $(KFM_FRAMES_H) $(STAGE0_PLANE_H) $(KFM_SOUNDS_H)
