# vdp2_nbg0_rbg0_combo - NBG0 seamless sky over an RBG0 Mode-7 floor.
COMBO_GEN_DIR := $(GENERATED_DIR)/vdp2_nbg0_rbg0_combo

EXAMPLE_ASSETS  := $(COMBO_GEN_DIR)/sky.c
EXAMPLE_HEADERS := $(COMBO_GEN_DIR)/sky.h

# Shares the ground example's seamless sky. 512 wide = the NBG0 plane, so the
# wrap has no seam; index 0 is reserved because NBG0 draws it transparent.
$(COMBO_GEN_DIR)/sky.c $(COMBO_GEN_DIR)/sky.h &: examples/vdp2_rbg0_ground/assets/sky.png tools/convert_indexed8.py
	@mkdir -p $(dir $@)
	$(PYTHON) tools/convert_indexed8.py --input $< --resize 512 128 --reserve-index0 --out-prefix $(COMBO_GEN_DIR)/sky
