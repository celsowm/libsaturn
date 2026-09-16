# vdp2_rbg0_ground - RBG0 infinite plane using the floor.tga asset, with a
# seamless panoramic sky on NBG0.
GROUND_GEN_DIR := $(GENERATED_DIR)/vdp2_rbg0_ground

EXAMPLE_ASSETS  := $(GROUND_GEN_DIR)/bg.c $(GROUND_GEN_DIR)/sky.c
EXAMPLE_HEADERS := $(GROUND_GEN_DIR)/bg.h $(GROUND_GEN_DIR)/sky.h

$(GROUND_GEN_DIR)/bg.c $(GROUND_GEN_DIR)/bg.h &: examples/vdp2_nbg0_image/assets/floor.tga tools/convert_indexed8.py
	@mkdir -p $(dir $@)
	$(PYTHON) tools/convert_indexed8.py --input $< --resize 64 64 --out-prefix $(GROUND_GEN_DIR)/bg

# 1024x512 seamless sky -> 512 wide (exactly the NBG0 plane, so the wrap has
# no seam). 128 tall keeps the whole zenith-to-horizon gradient in view.
# Index 0 is reserved: NBG0 draws it as transparent.
$(GROUND_GEN_DIR)/sky.c $(GROUND_GEN_DIR)/sky.h &: examples/vdp2_rbg0_ground/assets/sky.png tools/convert_indexed8.py
	@mkdir -p $(dir $@)
	$(PYTHON) tools/convert_indexed8.py --input $< --resize 512 128 --reserve-index0 --out-prefix $(GROUND_GEN_DIR)/sky
