# bullet_hell_stress: nothing but the code lives in the executable. The sprites (a SATL atlas with
# one residency group per stage), the three scrolling backgrounds and the music are built by
# tools/shmup_assets.py / tools/shmup_music.py from the vendored v4 asset subset and staged on the
# disc, from where the program streams them (bh_stream.c, sat_music).
#
#   BH_AUTOBENCH=1 make ...   builds a variant that starts the 128/256/512/1024-bullet benchmark by
#                             itself after three seconds (the harness and unattended emulator runs)
#   BH_FRAME_ON_SUBMIT=1      uses SAT_VDP1_FRAME_ON_SUBMIT instead of the default frame mode (the
#                             hardware erase and swap): it never shows a late list but erases with a
#                             full-screen polygon, which costs the VDP1 a third of a field in 640 x 224
#   BH_TATE_FB=1             the 320 x 224 hardware-rotated TATE (saturn/tate_fb.h): the VDP1 draws the
#                             224 x 448 field upright (rotation-8 framebuffer), the VDP2 turns it; upright art,
#                             no scrolling background, the HUD inside the field
#   BH_MUSIC_LIB=1            plays STAGE.PCM through sat_music + sat_music_use_disc_stream on the stream's
#                             scheduler instead of bh_stream's built-in raw feed (same drive sharing)
#   BH_EXTRA_CFLAGS=...       experiment knobs, e.g. -DBH_BENCH_LOOP=3 (repeat the 512 phase forever)
BH_GEN_DIR := $(GENERATED_DIR)/bullet_hell_stress
# The two variants pack different atlases (turned art or upright art), so they stage apart.
BH_VARIANT := $(if $(filter 1,$(BH_TATE_FB)),_fb)
BH_ISO_DIR := $(BH_GEN_DIR)/iso$(BH_VARIANT)
BH_PACK_DIR := $(BH_GEN_DIR)/pack$(BH_VARIANT)
BH_ASSETS := examples/bullet_hell_stress/assets
BH_TOOLS := tools/shmup_assets.py tools/shmup_music.py

# Group 0 is the common art; groups 1-3 are the enemy sets of stages 1-3 (red, violet, blue), 4 spare.
BH_GROUPS := \
	--group "enemy_*_red=1" --group "ship_enemy_red=1" --group "boss_core_red=1" \
	--group "enemy_*_violet=2" --group "ship_enemy_green=2" --group "boss_core_violet=2" \
	--group "enemy_*_blue=3" --group "ship_enemy_blue=3" --group "boss_core_blue=3" \
	--group "enemy_*_green=4" --group "enemy_*_orange=4" --group "boss_core_green=4"

# Hi-res 640 x 224 TATE: INDEX8 only, art turned a quarter clockwise at pack time.
# The rotation-framebuffer TATE draws the field upright (BH_TATE_FB=1): same 8 bit profile, no turn.
ifeq ($(BH_TATE_FB),1)
BH_PACK_ARGS := --name shmup --profile hires --orientation none $(BH_GROUPS)
CFLAGS += -DBH_TATE_FB=1
else
BH_PACK_ARGS := --name shmup --profile hires --orientation cw $(BH_GROUPS)
endif

ifeq ($(BH_MUSIC_LIB),1)
CFLAGS += -DBH_MUSIC_LIB=1
endif

ifeq ($(BH_AUTOBENCH),1)
CFLAGS += -DBH_AUTOBENCH=1
endif
ifeq ($(BH_FRAME_ON_SUBMIT),1)
CFLAGS += -DBH_FRAME_ON_SUBMIT=1
endif
CFLAGS += $(BH_EXTRA_CFLAGS)

EXAMPLE_HEADERS := $(BH_GEN_DIR)/shmup_atlas_ids.h $(BH_GEN_DIR)/bh_music.h
EXAMPLE_ISO_DIR := $(BH_ISO_DIR)
EXAMPLE_ISO_FILES := \
	$(BH_ISO_DIR)/SHMUP.ATL \
	$(BH_ISO_DIR)/STAGE1.BG $(BH_ISO_DIR)/STAGE2.BG $(BH_ISO_DIR)/STAGE3.BG \
	$(BH_ISO_DIR)/STAGE.PCM

$(BH_ISO_DIR)/SHMUP.ATL $(BH_GEN_DIR)/shmup_atlas_ids.h &: $(BH_TOOLS) $(BH_ASSETS)/manifest.json
	@mkdir -p $(BH_ISO_DIR) $(BH_PACK_DIR)
	$(PYTHON) tools/shmup_assets.py pack $(BH_ASSETS) --out $(BH_PACK_DIR) $(BH_PACK_ARGS)
	cp $(BH_PACK_DIR)/shmup.ATL $(BH_ISO_DIR)/SHMUP.ATL
	cp $(BH_PACK_DIR)/shmup_atlas_ids.h $(BH_GEN_DIR)/shmup_atlas_ids.h

$(BH_ISO_DIR)/STAGE1.BG $(BH_ISO_DIR)/STAGE2.BG $(BH_ISO_DIR)/STAGE3.BG &: $(BH_TOOLS) $(BH_ASSETS)/manifest.json
	@mkdir -p $(BH_ISO_DIR)
	$(PYTHON) tools/shmup_assets.py bg $(BH_ASSETS) --out $(BH_ISO_DIR) --orientation cw \
		--bg STAGE1.BG=metal --bg STAGE2.BG=starfield --bg STAGE3.BG=metal:bgr

$(BH_ISO_DIR)/STAGE.PCM $(BH_GEN_DIR)/bh_music.h &: tools/shmup_music.py
	@mkdir -p $(BH_ISO_DIR)
	$(PYTHON) tools/shmup_music.py --out $(BH_ISO_DIR)/STAGE.PCM --header $(BH_GEN_DIR)/bh_music.h
