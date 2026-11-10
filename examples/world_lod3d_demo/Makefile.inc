# Host-generated static HLOD fixture: original castle facade with door/window
# openings. The package is validated and byte-accounted by the build; its
# generated header makes the fixed-point/cell-count guards part of this SH-2
# example build without embedding converter state in the runtime planner.
WORLD_HLOD_GEN_DIR := $(GENERATED_DIR)/world_lod3d_demo
WORLD_HLOD_BIN := $(WORLD_HLOD_GEN_DIR)/world_demo.wlh
WORLD_HLOD_SOURCE := $(WORLD_HLOD_GEN_DIR)/world_lod3d_demo_hlod.c
WORLD_HLOD_HEADER := $(WORLD_HLOD_GEN_DIR)/world_lod3d_demo_hlod.h
WORLD_HLOD_REPORT := $(WORLD_HLOD_GEN_DIR)/world_demo.report.json
WORLD_HLOD_TOOLS := tools/build_world_hlod_demo.py $(wildcard tools/model_pipeline/*.py)
EXAMPLE_INCFLAGS += -I$(WORLD_HLOD_GEN_DIR)
EXAMPLE_HEADERS += $(WORLD_HLOD_HEADER)
EXAMPLE_ASSETS += $(WORLD_HLOD_SOURCE)

$(WORLD_HLOD_BIN) $(WORLD_HLOD_SOURCE) $(WORLD_HLOD_HEADER) $(WORLD_HLOD_REPORT) &: $(WORLD_HLOD_TOOLS)
	@mkdir -p $(WORLD_HLOD_GEN_DIR)
	$(PYTHON) tools/build_world_hlod_demo.py --output $(WORLD_HLOD_BIN) \
		--source $(WORLD_HLOD_SOURCE) --header $(WORLD_HLOD_HEADER) \
		--report $(WORLD_HLOD_REPORT)
