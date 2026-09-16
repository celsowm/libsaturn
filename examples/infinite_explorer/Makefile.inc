# Downloaded source art is reduced to indexed 8-bit Saturn assets at build time.
EXPLORER_ASSET_DIR := examples/infinite_explorer/assets
EXPLORER_GEN_DIR   := $(GENERATED_DIR)/infinite_explorer

EXAMPLE_ASSETS := $(EXPLORER_GEN_DIR)/terrain.c $(EXPLORER_GEN_DIR)/nebula.c $(EXPLORER_GEN_DIR)/spacecraft.c
EXAMPLE_HEADERS := $(EXPLORER_GEN_DIR)/terrain.h $(EXPLORER_GEN_DIR)/nebula.h $(EXPLORER_GEN_DIR)/spacecraft.h

$(EXPLORER_GEN_DIR)/terrain.c $(EXPLORER_GEN_DIR)/terrain.h &: $(EXPLORER_ASSET_DIR)/rocks_ground_02_cc0.jpg tools/convert_indexed8.py
	@mkdir -p $(dir $@)
	$(PYTHON) tools/convert_indexed8.py --input $< --resize 128 128 --out-prefix $(EXPLORER_GEN_DIR)/terrain

$(EXPLORER_GEN_DIR)/nebula.c $(EXPLORER_GEN_DIR)/nebula.h &: $(EXPLORER_ASSET_DIR)/nasa_peony_nebula.jpg tools/convert_indexed8.py
	@mkdir -p $(dir $@)
	$(PYTHON) tools/convert_indexed8.py --input $< --resize 128 96 --out-prefix $(EXPLORER_GEN_DIR)/nebula

$(EXPLORER_GEN_DIR)/spacecraft.c $(EXPLORER_GEN_DIR)/spacecraft.h &: $(EXPLORER_ASSET_DIR)/nasa_psyche_spacecraft.jpg tools/convert_indexed8.py
	@mkdir -p $(dir $@)
	$(PYTHON) tools/convert_indexed8.py --input $< --resize 64 64 --transparent-dark --out-prefix $(EXPLORER_GEN_DIR)/spacecraft
