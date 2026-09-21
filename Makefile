# ================================================================
# libsaturn Makefile - Build system for Sega Saturn
# ================================================================
# Usage:
#   make EXAMPLE=hello_world
#   make EXAMPLE=sega_bg IP_PROFILE=safe
#   make examples-all
#   make clean
# ================================================================
# Uso:
#   make EXAMPLE=hello_world
#   make EXAMPLE=sega_bg IP_PROFILE=safe
#   make examples-all
#   make clean
# ================================================================

# -- Toolchain --------------------------------------------------
TARGET      := sh2eb-elf
CC          := $(TARGET)-gcc
CXX         := $(TARGET)-g++
AR          := $(TARGET)-ar
OBJCOPY     := $(TARGET)-objcopy

# Python: prefer Windows one (has Pillow), fallback to MSYS2
PYTHON_WIN  := $(shell command -v python 2>/dev/null)
PYTHON_MSYS := $(shell command -v /usr/bin/python 2>/dev/null)
PYTHON      ?= $(PYTHON_WIN)

MKISOFS     := $(shell command -v mkisofs 2>/dev/null || command -v genisoimage 2>/dev/null || command -v xorrisofs 2>/dev/null)

# -- Paths ------------------------------------------------------
BUILD_DIR    := build
ISO_ROOT     := iso_root
GENERATED_DIR := $(BUILD_DIR)/generated
TOOLS        := tools

# -- Config -----------------------------------------------------
EXAMPLE      ?= hello_world
IP_PROFILE   ?= current
IP_TEMPLATE_KIND ?= yaul
APP_LOAD_ADDR_HEX := 06004000
MAX_APP_BIN_BYTES := 983040

VALID_IP_PROFILES  := current safe
VALID_IP_TEMPLATE_KINDS := yaul sbl minimal yaul_fixed region_free minimal_boot correct final

ifneq ($(filter $(IP_PROFILE),$(VALID_IP_PROFILES)),$(IP_PROFILE))
$(error Invalid IP_PROFILE '$(IP_PROFILE)'. Use: $(VALID_IP_PROFILES))
endif
ifneq ($(filter $(IP_TEMPLATE_KIND),$(VALID_IP_TEMPLATE_KINDS)),$(IP_TEMPLATE_KIND))
$(error Invalid IP_TEMPLATE_KIND '$(IP_TEMPLATE_KIND)'. Use: $(VALID_IP_TEMPLATE_KINDS))
endif

# -- IP template ------------------------------------------------
IP_TEMPLATE := assets/boot/ip_$(IP_TEMPLATE_KIND)_template.bin

# -- Compilacao -------------------------------------------------
BASE_CFLAGS := -m2 -mb -O2 -ffreestanding -fomit-frame-pointer -Wall -Wextra \
               -Iinclude -I. -I$(GENERATED_DIR)
CFLAGS      := $(BASE_CFLAGS)
# -MMD -MP make the compiler emit a .d file listing every header an object
# depends on. Without it a header edit leaves stale objects and binaries
# behind, and a test can "pass" against code that is no longer on disk.
DEPFLAGS    := -MMD -MP
CXXFLAGS    := $(CFLAGS) -std=c++20 -fno-exceptions -fno-rtti \
               -fno-threadsafe-statics -fno-use-cxa-atexit
ASFLAGS     := -m2 -mb
LDFLAGS     := -m2 -mb -nostdlib -Wl,-T,src/core/saturn.ld \
               -Wl,-Map,$(BUILD_DIR)/$(EXAMPLE).map -Wl,--gc-sections

# -- Biblioteca -------------------------------------------------
LIB_CPP_SRCS := $(wildcard src/core/*.cpp) $(wildcard src/hal/*.cpp)
LIB_C_SRCS   := $(wildcard src/core/*.c)
CRT_SRCS     := $(wildcard src/core/*.s)

LIB_CPP_OBJS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(LIB_CPP_SRCS))
LIB_C_OBJS   := $(patsubst %.c,$(BUILD_DIR)/%.o,$(LIB_C_SRCS))
CRT_OBJS     := $(patsubst %.s,$(BUILD_DIR)/%.o,$(CRT_SRCS))

LIBRARY := $(BUILD_DIR)/libsaturn.a

# -- Example (automatic discovery) ----------------------------
# Each example can have a Makefile.inc defining:
#   EXAMPLE_ASSETS  = list of assets (e.g. $(GENERATED_DIR)/sega_bg/bg.c)
#   EXAMPLE_HEADERS = generated headers
#   EXAMPLE_ISO_DIR = directory whose contents are copied into the ISO root
#   EXAMPLE_ISO_FILES = generated/checked-in files required by that directory
#   EXAMPLE_DEPS    = dependencies for generation
#   EXAMPLE_RESIZE  = width height (e.g. 320 224)
#   EXAMPLE_INPUT   = original image path
# ---------------------------------------------------------------
EXAMPLE_DIR     := examples/$(EXAMPLE)
EXAMPLE_HEADERS :=

# Shared sources under examples/common/ are OPT-IN: an example asks for them
# by name in its Makefile.inc, e.g.
#   EXAMPLE_COMMON_SRCS = examples/common/pacman_game.c
# Globbing examples/common/*.c instead would link every shared source into
# every example, so adding one game's helper would grow hello_world.
EXAMPLE_COMMON_SRCS :=

# Include example config if it exists
EXAMPLE_INC := $(EXAMPLE_DIR)/Makefile.inc
ifneq ($(wildcard $(EXAMPLE_INC)),)
  include $(EXAMPLE_INC)
endif

# Computed after the include so Makefile.inc can contribute to it.
EXAMPLE_SRCS    := $(wildcard $(EXAMPLE_DIR)/*.c) $(EXAMPLE_COMMON_SRCS)
EXAMPLE_OBJS    := $(patsubst %.c,$(BUILD_DIR)/%.o,$(EXAMPLE_SRCS))

ALL_APP_OBJS := $(EXAMPLE_OBJS) $(EXAMPLE_ASSETS:.c=.o)
ALL_HEADERS  := $(EXAMPLE_HEADERS)

# Generated asset headers are included by example sources, so make sure they
# exist before compiling any example object that may include them.
ifneq ($(strip $(ALL_HEADERS)),)
$(EXAMPLE_OBJS): $(ALL_HEADERS)
endif

# -- Artefatos --------------------------------------------------
ELF := $(BUILD_DIR)/$(EXAMPLE).elf
BIN := $(BUILD_DIR)/$(EXAMPLE).bin
ISO := $(BUILD_DIR)/$(EXAMPLE).iso
CUE := $(BUILD_DIR)/$(EXAMPLE).cue

# -- Available examples ---------------------------------------
EXAMPLES := $(filter-out common,$(notdir $(wildcard examples/*)))

.PHONY: all clean dirs check-tools examples-all list-examples bake test

all: check-tools dirs $(ELF) $(ISO) $(CUE) $(LIBRARY)

# -- Verificacoes -----------------------------------------------
check-tools:
	@if ! command -v $(CC) >/dev/null 2>&1; then \
		echo "Error: $(CC) not found in PATH"; exit 1; fi
	@if [ -z "$(MKISOFS)" ]; then \
		echo "Error: mkisofs/genisoimage/xorrisofs not found"; exit 1; fi
	@echo "[profiles] EXAMPLE=$(EXAMPLE) IP_PROFILE=$(IP_PROFILE) IP_TEMPLATE=$(IP_TEMPLATE_KIND)"

dirs:
	@mkdir -p $(BUILD_DIR) $(GENERATED_DIR) $(ISO_ROOT)
	@mkdir -p $(dir $(LIB_CPP_OBJS)) $(dir $(LIB_C_OBJS)) $(dir $(CRT_OBJS))
	@mkdir -p $(dir $(EXAMPLE_OBJS)) $(dir $(ALL_APP_OBJS))

# -- Asset generation ------------------------------------------
# If prebuilt/manifest.json exists in example AND hash matches, use prebuilt.
# Otherwise, convert via convert_indexed8.py (requires Pillow).
EXAMPLE_PREBUILT_DIR := $(EXAMPLE_DIR)/prebuilt
EXAMPLE_PREBUILT_MANIFEST := $(EXAMPLE_PREBUILT_DIR)/manifest.json

ifneq ($(wildcard $(EXAMPLE_PREBUILT_MANIFEST)),)
  # Check if prebuilt is valid (hash matches)
  EXAMPLE_PREBUILT_VALID := $(shell python $(TOOLS)/check-prebuilt.py $(EXAMPLE_PREBUILT_MANIFEST) 2>/dev/null)
  ifeq ($(EXAMPLE_PREBUILT_VALID),ok)
    # Use pre-converted assets
    PREBUILT_C := $(wildcard $(EXAMPLE_PREBUILT_DIR)/*.c)
    PREBUILT_H := $(wildcard $(EXAMPLE_PREBUILT_DIR)/*.h)
    # Prebuilt assets SEED build/generated rather than replacing it, and
    # EXAMPLE_ASSETS/EXAMPLE_HEADERS keep pointing at the generated paths the
    # example's Makefile.inc named. Pointing them at the prebuilt directory
    # instead, which is what this used to do, broke the build two ways:
    # ALL_APP_OBJS is computed from EXAMPLE_ASSETS further up this file and so
    # still asked for build/generated/<example>/<asset>.o, and examples include
    # their assets as <example>/<asset>.h, which only resolves against
    # -I$(GENERATED_DIR). Both happened to work only while a previous
    # non-prebuilt build had left copies behind; deleting build/generated made
    # the prebuilt path fail outright.
    $(shell mkdir -p $(GENERATED_DIR)/$(EXAMPLE) &&             cp -f $(PREBUILT_C) $(PREBUILT_H) $(GENERATED_DIR)/$(EXAMPLE)/ 2>/dev/null)
    $(info [assets] Prebuilt OK: $(EXAMPLE_PREBUILT_DIR))
  else
    # Hash changed or invalid, will convert
    $(info [assets] Prebuilt stale, will convert)
  endif
endif

# Generate compiled-in assets for examples that point at build/generated.
# Prebuilt examples keep their checked-in C/H files and skip this rule.
GENERATED_EXAMPLE_ASSET_TARGETS := $(filter $(BUILD_DIR)/generated/%,$(EXAMPLE_ASSETS) $(EXAMPLE_HEADERS))
ifneq ($(strip $(GENERATED_EXAMPLE_ASSET_TARGETS)),)
  ifneq ($(EXAMPLE_INPUT),)
  ifneq ($(EXAMPLE_RESIZE),)
  $(GENERATED_EXAMPLE_ASSET_TARGETS) &: $(EXAMPLE_INPUT) $(TOOLS)/convert_indexed8.py
	@mkdir -p $(dir $@)
	$(PYTHON) $(TOOLS)/convert_indexed8.py \
		--input $(EXAMPLE_INPUT) \
		--resize $(EXAMPLE_RESIZE) \
		--out-prefix $(EXAMPLE_ASSET_PREFIX)
  endif
  endif
endif

# Generic animated GLB compilation via tools/import_model.py --target saturn.
# An example opts in by defining MODEL_GLB in its Makefile.inc, e.g.:
#   MODEL_GLB           := examples/$(EXAMPLE)/assets/walk.glb
#   MODEL_OUT_PREFIX    := $(GENERATED_DIR)/$(EXAMPLE)/walk_model
#   MODEL_SYMBOL        := walk_model
# Optional: MODEL_SCALE, MODEL_TEXTURE_SCALE, MODEL_PALETTE_INDEX,
# MODEL_MAX_TEXTURE_WIDTH/HEIGHT, MODEL_SIMPLIFY (off|auto|TARGET),
# MODEL_QUALITY, MODEL_ANIMATION, MODEL_ANIMATION_FPS, MODEL_FLIP_X/Y/Z,
# MODEL_REVERSE_WINDING, MODEL_GENERATE_LODS, MODEL_FACE_COLORS (off|auto|on:
# solid lit polygon faces instead of textures), MODEL_MERGE_RIGID_MESHES,
# MODEL_LIGHT_DIR (x,y,z),
# MODEL_AMBIENT, MODEL_DIFFUSE.
# Model generation rebuilds when the GLB, the importer, or any
# model-pipeline module changes. Simplification/profile OPTION changes are
# not file dependencies: after editing them, remove the generated prefix
# (or touch the GLB) to force a rebuild.
MODEL_PIPELINE_SRCS := $(wildcard $(TOOLS)/model_pipeline/*.py)
ifdef MODEL_GLB
MODEL_SYMBOL              ?= $(notdir $(MODEL_OUT_PREFIX))
MODEL_SCALE               ?= 1.0
MODEL_TEXTURE_SCALE       ?= 1.0
MODEL_PALETTE_INDEX       ?= 1
MODEL_MAX_TEXTURE_WIDTH   ?= 504
MODEL_MAX_TEXTURE_HEIGHT  ?= 255
MODEL_SIMPLIFY            ?= auto
MODEL_QUALITY             ?= balanced
MODEL_ANIMATION           ?= all
MODEL_ANIMATION_FPS       ?= source
MODEL_FACE_COLORS         ?= off
MODEL_LIGHT_DIR           ?= -0.5,0.6,0.8
MODEL_AMBIENT             ?= 0.35
MODEL_DIFFUSE             ?= 0.75
MODEL_FLIP_FLAGS          :=
ifeq ($(MODEL_FLIP_X),1)
MODEL_FLIP_FLAGS += --flip-x
endif
ifeq ($(MODEL_FLIP_Y),1)
MODEL_FLIP_FLAGS += --flip-y
endif
ifeq ($(MODEL_FLIP_Z),1)
MODEL_FLIP_FLAGS += --flip-z
endif
ifeq ($(MODEL_REVERSE_WINDING),1)
MODEL_FLIP_FLAGS += --reverse-winding
endif
ifeq ($(MODEL_GENERATE_LODS),1)
MODEL_LOD_FLAG := --generate-lods
else
MODEL_LOD_FLAG :=
endif
ifeq ($(MODEL_MERGE_RIGID_MESHES),1)
MODEL_MERGE_RIGID_MESHES_FLAG := --merge-rigid-meshes
else
MODEL_MERGE_RIGID_MESHES_FLAG :=
endif
$(MODEL_OUT_PREFIX).c $(MODEL_OUT_PREFIX).h &: $(MODEL_GLB) $(TOOLS)/import_model.py $(MODEL_PIPELINE_SRCS)
	@if [ ! -f "$(MODEL_GLB)" ]; then \
		echo "error: animated source GLB missing: $(MODEL_GLB)"; \
		echo "Provide the example GLB (see its assets/LICENSE.txt) or build with repository fixtures."; \
		exit 1; fi
	@mkdir -p $(dir $@)
	$(PYTHON) $(TOOLS)/import_model.py \
		--input $(MODEL_GLB) \
		--target saturn \
		--out-prefix $(MODEL_OUT_PREFIX) \
		--symbol $(MODEL_SYMBOL) \
		--scale $(MODEL_SCALE) \
		--palette-index $(MODEL_PALETTE_INDEX) \
		--max-texture-width $(MODEL_MAX_TEXTURE_WIDTH) \
		--max-texture-height $(MODEL_MAX_TEXTURE_HEIGHT) \
		--texture-scale $(MODEL_TEXTURE_SCALE) \
		--simplify $(MODEL_SIMPLIFY) \
		--quality $(MODEL_QUALITY) \
		--animation $(MODEL_ANIMATION) \
		--animation-fps $(MODEL_ANIMATION_FPS) \
		--face-colors $(MODEL_FACE_COLORS) \
		--light-dir=$(MODEL_LIGHT_DIR) \
		--ambient $(MODEL_AMBIENT) \
		--diffuse $(MODEL_DIFFUSE) \
		--report $(MODEL_OUT_PREFIX).report.json \
		$(MODEL_FLIP_FLAGS) $(MODEL_LOD_FLAG) $(MODEL_MERGE_RIGID_MESHES_FLAG)
endif
# Generic 3D model generation via tools/import_model.py.
# An example opts in by defining MODEL_OBJ in its Makefile.inc, e.g.:
#   MODEL_OBJ         := examples/$(EXAMPLE)/assets/sonic.obj
#   MODEL_OUT_PREFIX  := $(GENERATED_DIR)/$(EXAMPLE)/sonic_model
#   MODEL_SYMBOL      := sonic_model
# Optional: MODEL_SCALE, MODEL_TEXTURE_SCALE, MODEL_PALETTE_INDEX,
# MODEL_MAX_TEXTURE_WIDTH/HEIGHT, MODEL_FLIP_X/Y/Z, MODEL_REVERSE_WINDING.
# The model rule coexists with the 2D converter above; an example uses one.
ifdef MODEL_OBJ
MODEL_SYMBOL              ?= $(notdir $(MODEL_OUT_PREFIX))
MODEL_SCALE               ?= 1.0
MODEL_TEXTURE_SCALE       ?= 1.0
MODEL_PALETTE_INDEX       ?= 1
MODEL_MAX_TEXTURE_WIDTH   ?= 504
MODEL_MAX_TEXTURE_HEIGHT  ?= 255
MODEL_FLIP_FLAGS          :=
ifeq ($(MODEL_FLIP_X),1)
MODEL_FLIP_FLAGS += --flip-x
endif
ifeq ($(MODEL_FLIP_Y),1)
MODEL_FLIP_FLAGS += --flip-y
endif
ifeq ($(MODEL_FLIP_Z),1)
MODEL_FLIP_FLAGS += --flip-z
endif
ifeq ($(MODEL_REVERSE_WINDING),1)
MODEL_FLIP_FLAGS += --reverse-winding
endif
$(MODEL_OUT_PREFIX).c $(MODEL_OUT_PREFIX).h &: $(MODEL_OBJ) $(TOOLS)/import_model.py
	@mkdir -p $(dir $@)
	$(PYTHON) $(TOOLS)/import_model.py \
		--input $(MODEL_OBJ) \
		--out-prefix $(MODEL_OUT_PREFIX) \
		--symbol $(MODEL_SYMBOL) \
		--scale $(MODEL_SCALE) \
		--palette-index $(MODEL_PALETTE_INDEX) \
		--max-texture-width $(MODEL_MAX_TEXTURE_WIDTH) \
		--max-texture-height $(MODEL_MAX_TEXTURE_HEIGHT) \
		--texture-scale $(MODEL_TEXTURE_SCALE) \
		$(MODEL_FLIP_FLAGS)
endif

# -- Compilation -------------------------------------------------
# Rule for C files (examples, library, generated assets)
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.s
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

# -- Linkagem ---------------------------------------------------
$(LIBRARY): $(LIB_CPP_OBJS) $(LIB_C_OBJS)
	$(AR) rcs $@ $^

$(ELF): $(CRT_OBJS) $(ALL_APP_OBJS) $(LIBRARY)
	$(CXX) $(LDFLAGS) -o $@ $(CRT_OBJS) $(ALL_APP_OBJS) -L$(BUILD_DIR) -lsaturn -lgcc
	@# crt0 never runs static constructors, so a global needing one is left
	@# null and fails in ways that look like a hardware bug. Catch it here.
	$(PYTHON) $(TOOLS)/check_no_init_array.py $@

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@
	@size=$$(wc -c < $@); \
	if [ $$size -gt $(MAX_APP_BIN_BYTES) ]; then \
		echo "Error: $(BIN) is $$size bytes; maximum is $(MAX_APP_BIN_BYTES) bytes"; \
		exit 1; \
	fi

$(ISO): $(BIN) $(EXAMPLE_ISO_FILES)
	@rm -rf $(ISO_ROOT)
	@mkdir -p $(ISO_ROOT)
	@cp $(BIN) $(ISO_ROOT)/0.BIN
	@cp $(IP_TEMPLATE) $(ISO_ROOT)/IP.BIN
	@if [ -n "$(EXAMPLE_ISO_DIR)" ]; then \
		cp -R "$(EXAMPLE_ISO_DIR)/." "$(ISO_ROOT)/"; \
	fi
	$(MKISOFS) -quiet -sysid "SEGA SATURN" -volid "LIBSATURN" \
		-volset "LIBSATURN" -publisher "LIBSATURN" -preparer "LIBSATURN" \
		-A "LIBSATURN" -G $(IP_TEMPLATE) -full-iso9660-filenames \
		-o $@ $(ISO_ROOT)

# -- CUE --------------------------------------------------------
$(CUE): $(ISO)
	$(PYTHON) $(TOOLS)/gen_cue.py \
		--iso-name $(EXAMPLE).iso \
		--cue-output $(CUE)

# -- Host tests -------------------------------------------------
# tests/host/*.cpp exercise the pure helpers in src/core/logic.hpp. They are
# built with the NATIVE compiler (not sh2eb-elf-g++) and run on the host, so
# they need no Saturn hardware, emulator or BIOS.
#   make test
HOST_CXX       ?= g++
HOST_CXXFLAGS  := -std=c++20 -Wall -Wextra -O1 -Iinclude -I.
HOST_TEST_SRCS := $(wildcard tests/host/*.cpp)
HOST_TEST_BINS := $(patsubst tests/host/%.cpp,$(BUILD_DIR)/tests/%,$(HOST_TEST_SRCS))
HOST_TOOL_TESTS := $(wildcard tests/tools/*.py)

# Some host tests exercise logic that lives in a library .cpp file rather than
# a header (e.g. font glyph tables), and stub out that file's hardware calls
# themselves (see the extern "C" stubs at the top of test_font_logic.cpp).
# List such extra sources per test name here.
HOST_TEST_EXTRA_test_font_logic := src/core/font_api.cpp
HOST_TEST_EXTRA_test_font_text_logic :=
HOST_TEST_EXTRA_test_audio_stream_logic :=
HOST_TEST_EXTRA_test_audio_stream_api := src/core/audio_stream_api.cpp src/core/audio_stream_runtime.cpp
HOST_TEST_EXTRA_test_music_api := src/core/music_api.cpp src/core/audio_stream_api.cpp src/core/audio_stream_runtime.cpp
HOST_TEST_EXTRA_test_save_api := src/core/save_api.cpp
HOST_TEST_EXTRA_test_save_schema := src/core/save_schema_api.cpp
HOST_TEST_EXTRA_test_resource_plan := src/core/resource_plan_api.cpp
HOST_TEST_EXTRA_test_hud := src/core/hud_api.cpp
HOST_TEST_EXTRA_test_sprite_anim := src/core/sprite_anim_api.cpp
HOST_TEST_EXTRA_test_view_cache := src/core/view_cache_api.cpp
HOST_TEST_EXTRA_test_surface3d := src/core/surface3d_api.cpp
HOST_TEST_EXTRA_test_vdp2_environment := src/core/vdp2_environment_api.cpp
HOST_TEST_EXTRA_test_ram_cart_api := src/core/ram_cart_api.cpp src/core/memory_api.cpp
HOST_TEST_EXTRA_test_ram_cart_1m := src/core/ram_cart_api.cpp src/core/memory_api.cpp
HOST_TEST_EXTRA_test_file_asset_logic := src/core/file_api.cpp src/core/asset_api.cpp src/core/file_asset_runtime.cpp src/core/texture_api.cpp src/core/runtime_state.cpp src/core/palette_registry.cpp src/core/texture_runtime.cpp
HOST_TEST_EXTRA_test_cdfs_logic := src/core/cd_api.cpp src/core/cdfs_api.cpp src/core/file_api.cpp src/core/file_asset_runtime.cpp
HOST_TEST_EXTRA_test_cd_block_api := src/hal/cd_block.cpp src/core/cd_api.cpp
HOST_TEST_EXTRA_test_math3d_logic := src/core/math3d_api.cpp
HOST_TEST_EXTRA_test_orbit_camera3d := src/core/orbit_camera3d_api.cpp src/core/math3d_api.cpp
HOST_TEST_EXTRA_test_follow_camera3d := src/core/follow_camera3d_api.cpp
HOST_TEST_EXTRA_test_transform3d := src/core/transform3d_api.cpp src/core/scene3d_api.cpp src/core/math3d_api.cpp
HOST_TEST_EXTRA_test_render3d_logic := src/core/math3d_api.cpp
HOST_TEST_EXTRA_test_voxel_terrain := src/core/voxel_terrain_api.cpp src/core/math3d_api.cpp
HOST_TEST_EXTRA_test_voxel_display_probe :=
HOST_TEST_EXTRA_test_render3d_effects := src/core/render3d_api.cpp src/core/math3d_api.cpp src/core/runtime_state.cpp
HOST_TEST_EXTRA_test_render3d_indexed := src/core/render3d_indexed_api.cpp
HOST_TEST_EXTRA_test_vdp2_bitmap := src/core/vdp2_bitmap_api.cpp
HOST_TEST_EXTRA_test_mesh3d_logic := src/core/math3d_api.cpp
HOST_TEST_EXTRA_test_mesh3d_textured := src/core/mesh3d_api.cpp src/core/math3d_api.cpp
HOST_TEST_EXTRA_test_vdp1_upload := src/core/vdp1_api.cpp src/core/runtime_state.cpp src/core/palette_registry.cpp
HOST_TEST_EXTRA_test_vdp1_clip := src/hal/vdp1.cpp
HOST_TEST_EXTRA_test_texture_api := src/core/texture_api.cpp src/core/runtime_state.cpp src/core/palette_registry.cpp src/core/texture_runtime.cpp
HOST_TEST_EXTRA_test_render2d_api := src/core/render2d_api.cpp src/core/render2d_runtime.cpp src/core/texture_api.cpp src/core/texture_runtime.cpp src/core/palette_registry.cpp src/core/runtime_state.cpp
HOST_TEST_EXTRA_test_input_api := src/core/input_api.cpp src/core/input_runtime.cpp src/core/runtime_state.cpp
HOST_TEST_EXTRA_test_model3d_logic := src/core/model3d_api.cpp src/core/mesh3d_api.cpp src/core/math3d_api.cpp
HOST_TEST_EXTRA_test_scene3d_api := src/core/scene3d_api.cpp src/core/model3d_api.cpp src/core/math3d_api.cpp
HOST_TEST_EXTRA_test_scene_api := src/core/scene_api.cpp src/core/math3d_api.cpp
HOST_TEST_EXTRA_test_scene3d_faces_api := src/core/scene3d_faces_api.cpp
HOST_TEST_EXTRA_test_scene3d_material_pool := src/core/scene3d_material_pool_api.cpp
HOST_TEST_EXTRA_test_anim3d_logic := src/core/anim3d_api.cpp src/core/model3d_api.cpp src/core/mesh3d_api.cpp src/core/math3d_api.cpp
HOST_TEST_EXTRA_test_pacman_game := examples/common/pacman_game.c src/core/grid_api.cpp src/core/collide2d_api.cpp src/core/math3d_api.cpp
HOST_TEST_EXTRA_test_skybridge_game := src/core/surface3d_api.cpp
HOST_TEST_EXTRA_test_skybridge_fade := src/core/surface3d_api.cpp
HOST_TEST_EXTRA_test_collide2d_logic := src/core/collide2d_api.cpp
HOST_TEST_EXTRA_test_spatial_logic := src/core/spatial_api.cpp src/core/collide2d_api.cpp
HOST_TEST_EXTRA_test_physics_logic := src/core/collide2d_api.cpp src/core/grid_api.cpp
HOST_TEST_EXTRA_test_collide3d_logic :=
HOST_TEST_EXTRA_test_spatial3_api := src/core/spatial3_api.cpp src/core/collide3d_api.cpp

$(BUILD_DIR)/tests/%: tests/host/%.cpp
	@mkdir -p $(dir $@)
	$(HOST_CXX) $(HOST_CXXFLAGS) $(DEPFLAGS) $< $(HOST_TEST_EXTRA_$*) -o $@

test: $(HOST_TEST_BINS)
	@fail=0; \
	for t in $(HOST_TEST_BINS); do \
		if ! ./$$t; then fail=1; fi; \
	done; \
	if [ $$fail -ne 0 ]; then echo "[test] FAILED"; exit 1; fi; \
	echo "[test] all host tests passed"
	@for t in $(HOST_TOOL_TESTS); do \
		echo "[test] $$t"; \
		$(PYTHON) $$t || exit 1; \
	done

# -- Alvos utilitarios ------------------------------------------
list-examples:
	@echo "Available examples:"
	@for e in $(EXAMPLES); do echo "  $$e"; done

examples-all:
	@for e in $(filter-out $(EXAMPLES_ALL_SKIP),$(EXAMPLES)); do \
		echo "[build] $$e"; \
		$(MAKE) --no-print-directory EXAMPLE=$$e all || exit $$?; \
	done

clean:
	rm -rf $(BUILD_DIR) $(ISO_ROOT)
