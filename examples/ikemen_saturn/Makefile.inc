# ikemen_saturn - Ikemen GO screenpack assets (KFM + stage0) on Saturn.
# Real sprites/palettes converted offline by tools/ikemen_sff from the
# Ikemen-GO-Screenpack repo (see README for attribution). The runtime
# consumes generated metadata plus disc-resident packed sprite payloads.
# Fight logic stays in examples/ikemen_saturn/ikemen_fight.c for host tests.

IKEMEN_GEN_DIR := $(GENERATED_DIR)/ikemen_saturn
IKEMEN_ISO_DIR := $(IKEMEN_GEN_DIR)/iso
KFM_FRAMES_C := $(IKEMEN_GEN_DIR)/kfm_frames.c
KFM_FRAMES_H := $(IKEMEN_GEN_DIR)/kfm_frames.h
KFM_SPRITES_BIN := $(IKEMEN_ISO_DIR)/KFM_SPR.BIN
KFM_ZSS_FRAMES_C := $(IKEMEN_GEN_DIR)/kfm_zss_frames.c
KFM_ZSS_FRAMES_H := $(IKEMEN_GEN_DIR)/kfm_zss_frames.h
KFM_ZSS_SPRITES_BIN := $(IKEMEN_ISO_DIR)/KFM_ZSS.BIN
FIGHTFX_FRAMES_C := $(IKEMEN_GEN_DIR)/fightfx_frames.c
FIGHTFX_FRAMES_H := $(IKEMEN_GEN_DIR)/fightfx_frames.h
FIGHTFX_SPRITES_BIN := $(IKEMEN_ISO_DIR)/FIGHTFX.BIN
STAGE0_PLANE_C := $(IKEMEN_GEN_DIR)/stage0_plane.c
STAGE0_PLANE_H := $(IKEMEN_GEN_DIR)/stage0_plane.h
KFM_SOUNDS_C := $(IKEMEN_GEN_DIR)/kfm_sounds.c
KFM_SOUNDS_H := $(IKEMEN_GEN_DIR)/kfm_sounds.h
KFM_COMMANDS_C := $(IKEMEN_GEN_DIR)/kfm_commands.c
KFM_COMMANDS_H := $(IKEMEN_GEN_DIR)/kfm_commands.h
KFM_STATE_RULES_C := $(IKEMEN_GEN_DIR)/kfm_state_rules.c
KFM_STATE_RULES_H := $(IKEMEN_GEN_DIR)/kfm_state_rules.h
KFM_CNS_C := $(IKEMEN_GEN_DIR)/kfm_cns.c
KFM_CNS_H := $(IKEMEN_GEN_DIR)/kfm_cns.h
IKEMEN_SFF_TOOL_SRCS := $(wildcard tools/ikemen_sff/*.py)
IKEMEN_SND_TOOL := tools/ikemen_snd.py
IKEMEN_CMD_TOOL := tools/ikemen_cmd.py
IKEMEN_STATE_RULE_TOOL := tools/ikemen_state_rules.py
IKEMEN_CNS_TOOL := tools/ikemen_cns.py

KFM_SFF := .external/Ikemen-GO-Screenpack/chars/kfm/kfm.sff
KFM_AIR := .external/Ikemen-GO-Screenpack/chars/kfm/kfm.air
KFM_ZSS_SFF := .external/Ikemen-GO-Screenpack/chars/kfm_zss/kfm.sff
KFM_ZSS_AIR := .external/Ikemen-GO-Screenpack/chars/kfm_zss/kfm.air
KFM_SND := .external/Ikemen-GO-Screenpack/chars/kfm/kfm.snd
KFM_CMD := .external/Ikemen-GO-Screenpack/chars/kfm/kfm.cmd
KFM_CNS := .external/Ikemen-GO-Screenpack/chars/kfm/kfm.cns
COMMON_CNS_ZSS := .external/Ikemen-GO/data/common1.cns.zss
COMMON_SND := .external/Ikemen-GO-Screenpack/data/common.snd
FIGHTFX_SFF := .external/Ikemen-GO-Screenpack/data/fightfx.sff
FIGHTFX_AIR := .external/Ikemen-GO-Screenpack/data/fightfx.air
STAGE0_SFF := .external/Ikemen-GO-Screenpack/stages/stage0.sff

IKEMEN_FIGHTFX_ACTIONS := 0,1,2,3,40
IKEMEN_CHARACTER_ACTIONS := 0,10,11,12,20,21,40,41,42,43,47,100,105,120,121,122,130,131,132,140,141,142,150,151,152,200,210,230,240,400,410,430,440,600,610,630,640,800,810,820,821,1000,1010,1020,1025,1027,1050,1051,1052,1055,1056,1060,1061,1070,1071,1100,1110,1120,1200,1210,1220,1300,1310,1320,1330,1340,1350,1351,1400,1410,1420,3000,3050,3051,5000,5001,5002,5010,5011,5012,5020,5021,5022,5030,5035,5040,5050,5070,5080,5090,5100,5160,5110,5120,5150,5200,5210

$(KFM_FRAMES_C) $(KFM_FRAMES_H) $(KFM_SPRITES_BIN) &: $(KFM_SFF) $(KFM_AIR) $(IKEMEN_SFF_TOOL_SRCS)
	@mkdir -p $(IKEMEN_GEN_DIR) $(IKEMEN_ISO_DIR)
	$(PYTHON) -m tools.ikemen_sff char \
		--sff $(KFM_SFF) --air $(KFM_AIR) \
		--actions $(IKEMEN_CHARACTER_ACTIONS) \
		--palettes "1,1;1,4" \
		--out-prefix $(IKEMEN_GEN_DIR)/kfm --symbol kfm \
		--sprite-bin $(KFM_SPRITES_BIN)

$(KFM_ZSS_FRAMES_C) $(KFM_ZSS_FRAMES_H) $(KFM_ZSS_SPRITES_BIN) &: $(KFM_ZSS_SFF) $(KFM_ZSS_AIR) $(IKEMEN_SFF_TOOL_SRCS)
	@mkdir -p $(IKEMEN_GEN_DIR) $(IKEMEN_ISO_DIR)
	$(PYTHON) -m tools.ikemen_sff char \
		--sff $(KFM_ZSS_SFF) --air $(KFM_ZSS_AIR) \
		--actions $(IKEMEN_CHARACTER_ACTIONS) \
		--palettes "1,1" \
		--out-prefix $(IKEMEN_GEN_DIR)/kfm_zss --symbol kfm_zss \
		--sprite-bin $(KFM_ZSS_SPRITES_BIN)

$(FIGHTFX_FRAMES_C) $(FIGHTFX_FRAMES_H) $(FIGHTFX_SPRITES_BIN) &: $(FIGHTFX_SFF) $(FIGHTFX_AIR) $(IKEMEN_SFF_TOOL_SRCS)
	@mkdir -p $(IKEMEN_GEN_DIR) $(IKEMEN_ISO_DIR)
	$(PYTHON) -m tools.ikemen_sff fx \
		--sff $(FIGHTFX_SFF) --air $(FIGHTFX_AIR) \
		--actions $(IKEMEN_FIGHTFX_ACTIONS) \
		--out-prefix $(IKEMEN_GEN_DIR)/fightfx --symbol fightfx \
		--sprite-bin $(FIGHTFX_SPRITES_BIN)

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
		--sound strong_punch_whiff $(KFM_SND) 0 4 \
		--sound sweep_whiff $(KFM_SND) 0 2 \
		--sound punch_hit $(COMMON_SND) 5 0 \
		--sound kick_hit $(COMMON_SND) 5 1 \
		--sound strong_hit $(COMMON_SND) 5 2 \
		--sound air_strong_hit $(COMMON_SND) 5 3 \
		--sound reversal_hit $(COMMON_SND) 6 0

$(KFM_COMMANDS_C) $(KFM_COMMANDS_H) &: $(KFM_CMD) $(IKEMEN_CMD_TOOL)
	@mkdir -p $(IKEMEN_GEN_DIR)
	$(PYTHON) $(IKEMEN_CMD_TOOL) \
		--cmd $(KFM_CMD) \
		--out-prefix $(IKEMEN_GEN_DIR)/kfm_commands --symbol kfm

$(KFM_STATE_RULES_C) $(KFM_STATE_RULES_H) &: $(KFM_CMD) $(IKEMEN_STATE_RULE_TOOL) $(IKEMEN_CMD_TOOL)
	@mkdir -p $(IKEMEN_GEN_DIR)
	$(PYTHON) -m tools.ikemen_state_rules \
		--cmd $(KFM_CMD) \
		--targets 100,105,200,210,230,240,400,410,430,440,600,610,630,640,800,1000,1010,1020,1050,1060,1070,1100,1110,1120,1200,1210,1220,1300,1320,1340,1400,1410,1420,3000,3050 \
		--out-prefix $(IKEMEN_GEN_DIR)/kfm_state_rules --symbol kfm

$(KFM_CNS_C) $(KFM_CNS_H) &: $(KFM_CNS) $(COMMON_CNS_ZSS) $(IKEMEN_CNS_TOOL)
	@mkdir -p $(IKEMEN_GEN_DIR)
	$(PYTHON) $(IKEMEN_CNS_TOOL) \
		--cns $(KFM_CNS) --states 200,210,230,240,400,410,430,440,600,610,630,640,800,810,820,821,1000,1010,1020,1025,1026,1027,1028,1050,1051,1052,1055,1056,1060,1061,1070,1071,1075,1100,1110,1120,1200,1210,1220,1300,1310,1320,1330,1340,1350,1351,1400,1410,1420,3000,3050,3051 \
		--common-zss $(COMMON_CNS_ZSS) --common-states 0,10,11,12,20,40,45,50,51,52,100,105,106,120,130,131,132,140,150,151,152,153,154,155,5000,5001,5010,5011,5020,5030,5035,5040,5050,5070,5071,5080,5081,5100,5101,5110,5120,5150,5200,5201,5210 \
		--out-prefix $(IKEMEN_GEN_DIR)/kfm_cns --symbol kfm

EXAMPLE_ASSETS := $(KFM_FRAMES_C) $(KFM_ZSS_FRAMES_C) $(FIGHTFX_FRAMES_C) $(STAGE0_PLANE_C) $(KFM_SOUNDS_C) $(KFM_COMMANDS_C) $(KFM_STATE_RULES_C) $(KFM_CNS_C)
EXAMPLE_HEADERS += $(KFM_FRAMES_H) $(KFM_ZSS_FRAMES_H) $(FIGHTFX_FRAMES_H) $(STAGE0_PLANE_H) $(KFM_SOUNDS_H) $(KFM_COMMANDS_H) $(KFM_STATE_RULES_H) $(KFM_CNS_H)
EXAMPLE_ISO_DIR := $(IKEMEN_ISO_DIR)
EXAMPLE_ISO_FILES := $(KFM_SPRITES_BIN) $(KFM_ZSS_SPRITES_BIN) $(FIGHTFX_SPRITES_BIN)
