# The stage data of high_speed_platformer: gen_stage.py writes the stage2d spec and the layout,
# tools/stage2d_tool.py compiles the spec. Everything lands in the build tree, nothing generated is
# checked in. Included by Makefile.inc (the Saturn build) and host_test.mk (the host simulation),
# so the rule is written once.
ifndef HSP_STAGE_RULES
HSP_STAGE_RULES := 1

HSP_DIR := examples/high_speed_platformer
HSP_GEN_DIR := $(BUILD_DIR)/generated/high_speed_platformer
HSP_STAGE_FILES := $(HSP_GEN_DIR)/stage.h $(HSP_GEN_DIR)/stage.c $(HSP_GEN_DIR)/layout.h $(HSP_GEN_DIR)/layout.c

$(HSP_STAGE_FILES) &: $(HSP_DIR)/tools/gen_stage.py tools/stage2d_tool.py $(wildcard tools/stage2d/*.py)
	@mkdir -p $(HSP_GEN_DIR)
	$(PYTHON) $(HSP_DIR)/tools/gen_stage.py --out-dir $(HSP_GEN_DIR)
	$(PYTHON) tools/stage2d_tool.py build $(HSP_GEN_DIR)/stage_spec.json --out-dir $(HSP_GEN_DIR)

endif
