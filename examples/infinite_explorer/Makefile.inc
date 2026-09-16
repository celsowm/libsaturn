# Real CC0/public-domain visual and audio sources are converted to Saturn assets at build time.
EXPLORER_ASSET_DIR := examples/infinite_explorer/assets
EXPLORER_GEN_DIR   := $(GENERATED_DIR)/infinite_explorer
EXPLORER_CACHE_DIR := $(EXPLORER_GEN_DIR)/cc0_cache

EXPLORER_HORIZON_PREFIX := $(EXPLORER_GEN_DIR)/horizon
EXPLORER_AUDIO_PREFIX   := $(EXPLORER_GEN_DIR)/audio_data
EXPLORER_REQUIRE_REAL_FLAG := $(if $(filter 1 yes true,$(INFINITE_EXPLORER_REQUIRE_REAL)),--require-real,)

EXAMPLE_ASSETS := \
	$(EXPLORER_GEN_DIR)/terrain.c \
	$(EXPLORER_HORIZON_PREFIX).c \
	$(EXPLORER_GEN_DIR)/spacecraft.c \
	$(EXPLORER_AUDIO_PREFIX).c
EXAMPLE_HEADERS := \
	$(EXPLORER_GEN_DIR)/terrain.h \
	$(EXPLORER_HORIZON_PREFIX).h \
	$(EXPLORER_GEN_DIR)/spacecraft.h \
	$(EXPLORER_AUDIO_PREFIX).h

$(EXPLORER_GEN_DIR)/terrain.c $(EXPLORER_GEN_DIR)/terrain.h &: $(EXPLORER_ASSET_DIR)/rocks_ground_02_cc0.jpg tools/convert_indexed8.py
	@mkdir -p $(dir $@)
	$(PYTHON) tools/convert_indexed8.py --input $< --resize 128 128 --out-prefix $(EXPLORER_GEN_DIR)/terrain

$(EXPLORER_HORIZON_PREFIX).c $(EXPLORER_HORIZON_PREFIX).h &: tools/generate_infinite_explorer_horizon.py tools/saturn_asset_common.py
	@mkdir -p $(dir $@)
	$(PYTHON) tools/generate_infinite_explorer_horizon.py \
		--out-c $(EXPLORER_HORIZON_PREFIX).c \
		--out-h $(EXPLORER_HORIZON_PREFIX).h \
		--cache-dir $(EXPLORER_CACHE_DIR) \
		$(EXPLORER_REQUIRE_REAL_FLAG)

$(EXPLORER_GEN_DIR)/spacecraft.c $(EXPLORER_GEN_DIR)/spacecraft.h &: $(EXPLORER_ASSET_DIR)/nasa_psyche_spacecraft.jpg tools/convert_indexed8.py
	@mkdir -p $(dir $@)
	$(PYTHON) tools/convert_indexed8.py --input $< --resize 64 64 --transparent-dark --out-prefix $(EXPLORER_GEN_DIR)/spacecraft

$(EXPLORER_AUDIO_PREFIX).c $(EXPLORER_AUDIO_PREFIX).h &: tools/generate_infinite_explorer_audio.py
	@mkdir -p $(dir $@)
	$(PYTHON) tools/generate_infinite_explorer_audio.py \
		--out-c $(EXPLORER_AUDIO_PREFIX).c \
		--out-h $(EXPLORER_AUDIO_PREFIX).h \
		--cache-dir $(EXPLORER_CACHE_DIR) \
		$(EXPLORER_REQUIRE_REAL_FLAG)
