# vdp2_parallax_lab - four hand-painted layers, four VDP2 scroll planes.
PLAB_GEN := $(GENERATED_DIR)/vdp2_parallax_lab

EXAMPLE_ASSETS  := $(PLAB_GEN)/assets.c
EXAMPLE_HEADERS := $(PLAB_GEN)/assets.h

$(PLAB_GEN)/assets.c $(PLAB_GEN)/assets.h &: \
    examples/vdp2_parallax_lab/assets/sky.png \
    examples/vdp2_parallax_lab/assets/mountains.png \
    examples/vdp2_parallax_lab/assets/sea.png \
    examples/vdp2_parallax_lab/assets/foreground.png \
    tools/convert_vdp2_parallax_lab.py
	@mkdir -p $(dir $@)
	$(PYTHON) tools/convert_vdp2_parallax_lab.py \
		--assets examples/vdp2_parallax_lab/assets \
		--out-prefix $(PLAB_GEN)/assets
