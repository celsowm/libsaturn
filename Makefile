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
APP_LOAD_ADDR_HEX := $(shell $(PYTHON) tools/memory_layout.py app_load_hex 2>/dev/null || echo 06004000)
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
IP_GENERATED := $(BUILD_DIR)/$(EXAMPLE).IP.BIN

# -- Compilacao -------------------------------------------------
BASE_CFLAGS := -m2 -mb -O2 -ffreestanding -fomit-frame-pointer -Wall -Wextra \
               -Iinclude -I. -I$(GENERATED_DIR)
CFLAGS      := $(BASE_CFLAGS)
ifeq ($(EXAMPLE),parallel_runtime)
ifneq ($(strip $(PARALLEL_RUNTIME_GEOMETRY_OBJECTS)),)
CFLAGS      += -DSAT_PARALLEL_RUNTIME_GEOMETRY_OBJECTS=$(PARALLEL_RUNTIME_GEOMETRY_OBJECTS)
endif
ifneq ($(strip $(PARALLEL_RUNTIME_FACES_PER_OBJECT)),)
CFLAGS      += -DSAT_PARALLEL_RUNTIME_FACES_PER_OBJECT=$(PARALLEL_RUNTIME_FACES_PER_OBJECT)
endif
ifneq ($(strip $(PARALLEL_RUNTIME_DEFAULT_MODE)),)
CFLAGS      += -DSAT_PARALLEL_RUNTIME_DEFAULT_MODE=$(PARALLEL_RUNTIME_DEFAULT_MODE)
endif
ifneq ($(strip $(SAT_PARALLEL_RUNTIME_VALIDATION)),)
CFLAGS      += -DSAT_PARALLEL_RUNTIME_VALIDATION=1 -DSAT_PROFILE_METRICS=1
endif
endif
ifeq ($(EXAMPLE),skybridge_3d)
ifneq ($(strip $(SAT_SKYBRIDGE_PARALLEL_MODE)),)
CFLAGS      += -DSAT_SKYBRIDGE_PARALLEL_MODE=$(SAT_SKYBRIDGE_PARALLEL_MODE)
endif

ifneq ($(strip $(SAT_SKYBRIDGE_VALIDATION)),)
CFLAGS      += -DSAT_SKYBRIDGE_VALIDATION=1 -DSAT_PROFILE_METRICS=1
endif
ifneq ($(strip $(SAT_SKYBRIDGE_FORCE_GEM_SPLIT)),)
CFLAGS      += -DSAT_SKYBRIDGE_FORCE_GEM_SPLIT=1
endif
endif
ifeq ($(EXAMPLE),parallel_runtime)
.PHONY: parallel-runtime-profile-flags
parallel-runtime-profile-flags:
$(BUILD_DIR)/examples/parallel_runtime/main.o \
$(BUILD_DIR)/src/graphics/3d/scene/parallel.o: parallel-runtime-profile-flags
endif
ifneq ($(strip $(SAT_PARALLEL_TEST_FAULT)),)
CFLAGS      += -DSAT_PARALLEL_TEST_FAULT=$(SAT_PARALLEL_TEST_FAULT)
endif
# -MMD -MP make the compiler emit a .d file listing every header an object
# depends on. Without it a header edit leaves stale objects and binaries
# behind, and a test can "pass" against code that is no longer on disk.
DEPFLAGS    := -MMD -MP
CXXFLAGS    := $(CFLAGS) -std=c++20 -fno-exceptions -fno-rtti \
               -fno-threadsafe-statics -fno-use-cxa-atexit
ASFLAGS     := -m2 -mb
LDFLAGS     := -m2 -mb -nostdlib -Wl,-T,src/core/startup/saturn.ld \
               -Wl,-Map,$(BUILD_DIR)/$(EXAMPLE).map -Wl,--gc-sections

# -- Biblioteca -------------------------------------------------
# Source files are intentionally discovered recursively: implementation ownership
# is expressed by the directory tree, so a shallow wildcard would silently omit
# every subsystem below src/core, src/graphics, src/audio, and src/hal.
rwildcard = $(foreach d,$(wildcard $(1)/*),$(call rwildcard,$(d),$(2)) $(wildcard $(d)/$(2)))
LIB_CPP_SRCS := $(call rwildcard,src,*.cpp)
LIB_C_SRCS   := $(call rwildcard,src,*.c)
CRT_SRCS     := $(call rwildcard,src,*.s)

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

# Pulls in the per-object dependency files DEPFLAGS (-MMD -MP) writes, so a
# header edit rebuilds every object that #includes it -- library, example and
# generated-asset alike -- not just the one .c/.cpp file whose own mtime
# changed. Without this, `-MMD -MP` writes the .d files but nothing ever
# reads them, which is worse than not having them at all: it looks like
# header dependencies are tracked when they are not. A stale object from a
# shared header (examples/common/pacman_game.h, in the case that found this)
# links fine and can misbehave at runtime in ways that look nothing like a
# build problem, while `make` reports the build clean.
# `-include`, not `include`, so a .d file that does not exist yet (nothing
# has been built) is skipped instead of stopping the build.
-include $(LIB_C_OBJS:.o=.d) $(LIB_CPP_OBJS:.o=.d) $(ALL_APP_OBJS:.o=.d)

# Generated asset headers are included by example sources, so make sure they
# exist before compiling any example object that may include them.
ifneq ($(strip $(ALL_HEADERS)),)
$(EXAMPLE_OBJS): $(ALL_HEADERS)
endif

# Profiling is a reusable library feature: the game owns its own validation
# selection, while generic modules know only SAT_PROFILE_METRICS. Make cannot
# detect changed command-line flags, so rebuild profiling translation units.
ifneq ($(filter skybridge_3d parallel_runtime,$(EXAMPLE)),)
.PHONY: profile-flags
profile-flags:
$(BUILD_DIR)/src/core/parallel/executor.o \
$(BUILD_DIR)/src/graphics/3d/scene/parallel.o \
$(BUILD_DIR)/src/graphics/3d/scene/faces.o \
$(BUILD_DIR)/src/hal/vdp1/vdp1.o: profile-flags
ifeq ($(EXAMPLE),skybridge_3d)
$(BUILD_DIR)/examples/skybridge_3d/main.o: profile-flags
else
$(BUILD_DIR)/examples/parallel_runtime/main.o: profile-flags
endif
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
# MODEL_AMBIENT, MODEL_DIFFUSE. MODEL_MERGE_QUADS (off|on) draws adjacent
# triangle pairs as one VDP1 quad; MODEL_QUAD_MAX_TEXEL_ERROR and
# MODEL_QUAD_MAX_FOLD_DEG bound what a merge may change.
# MODEL_MATERIAL_WEIGHTS is a space-separated NAME=W list; W below 1 spends
# fewer simplified triangles on that material. MODEL_WELD_VERTICES (off|on)
# drops the UV-split vertex copies a textured model never needs at runtime.
# MODEL_TEXTURE_FORMAT (indexed8|lut4): lut4 bakes 4-bit texels with a
# 15-color VDP1 lookup table per face, half the VRAM of indexed8.
# MODEL_LOCALITY_ORDER (off|on) orders faces along the model and vertices by
# first use, so a face list split between CPUs splits its vertices too.
# MODEL_MAX_POSE_STREAM_BYTES can raise the per-asset baked-pose cap when
# the resulting executable still fits the Saturn work-RAM budget.
# MODEL_HUD_RESERVE is the example-specific VDP1 command count reserved
# for text or overlay work that can draw alongside the model.
# A lightweight signature check runs for each model build. It hashes input
# contents, referenced sidecars, importer options and conversion tools; the
# expensive import runs only when that signature or an output changes.
MODEL_PIPELINE_SRCS := $(wildcard $(TOOLS)/model_pipeline/*.py)
MODEL_IMPORT_SRCS := $(TOOLS)/import_model.py $(TOOLS)/saturn_asset_common.py $(MODEL_PIPELINE_SRCS)
MODEL_IMPORT_INCREMENTAL_FLAG := --incremental
MODEL_IMPORT_FORCE_FLAG :=
ifneq ($(findstring B,$(firstword $(MAKEFLAGS))),)
MODEL_IMPORT_FORCE_FLAG := --force-import
endif
.PHONY: FORCE_MODEL_IMPORT_SIGNATURE
FORCE_MODEL_IMPORT_SIGNATURE:
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
MODEL_MAX_POSE_STREAM_BYTES ?= 262144
MODEL_HUD_RESERVE           ?= 128
MODEL_MERGE_QUADS           ?= off
MODEL_QUAD_MAX_TEXEL_ERROR  ?= 1.0
MODEL_QUAD_MAX_FOLD_DEG     ?= 30
MODEL_WELD_VERTICES         ?= off
MODEL_TEXTURE_FORMAT        ?= indexed8
MODEL_LOCALITY_ORDER        ?= off
MODEL_SIGNATURE_FILE := $(MODEL_OUT_PREFIX).signature.json
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
MODEL_IMPORT_ARGS = \
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
	--max-pose-stream-bytes $(MODEL_MAX_POSE_STREAM_BYTES) \
	--hud-reserve $(MODEL_HUD_RESERVE) \
	--merge-quads $(MODEL_MERGE_QUADS) \
	--quad-max-texel-error $(MODEL_QUAD_MAX_TEXEL_ERROR) \
	--quad-max-fold-deg $(MODEL_QUAD_MAX_FOLD_DEG) \
	$(foreach w,$(MODEL_MATERIAL_WEIGHTS),--material-weight $(w)) \
	--weld-vertices $(MODEL_WELD_VERTICES) \
	--texture-format $(MODEL_TEXTURE_FORMAT) \
	--locality-order $(MODEL_LOCALITY_ORDER) \
	--report $(MODEL_OUT_PREFIX).report.json \
	$(MODEL_FLIP_FLAGS) $(MODEL_LOD_FLAG) $(MODEL_MERGE_RIGID_MESHES_FLAG)
$(MODEL_SIGNATURE_FILE): FORCE_MODEL_IMPORT_SIGNATURE $(MODEL_GLB) $(MODEL_IMPORT_SRCS)
	@mkdir -p $(dir $@)
	$(PYTHON) $(TOOLS)/import_model.py $(MODEL_IMPORT_ARGS) \
		--signature-only --signature-file $@
$(MODEL_OUT_PREFIX).c $(MODEL_OUT_PREFIX).h $(MODEL_OUT_PREFIX).report.json $(MODEL_OUT_PREFIX).import.json &: $(MODEL_GLB) $(MODEL_IMPORT_SRCS) $(MODEL_SIGNATURE_FILE)
	@if [ ! -f "$(MODEL_GLB)" ]; then \
		echo "error: animated source GLB missing: $(MODEL_GLB)"; \
		echo "Provide the example GLB (see its assets/LICENSE.txt) or build with repository fixtures."; \
		exit 1; fi
	@mkdir -p $(dir $@)
	$(PYTHON) $(TOOLS)/import_model.py $(MODEL_IMPORT_ARGS) $(MODEL_IMPORT_INCREMENTAL_FLAG) $(MODEL_IMPORT_FORCE_FLAG)
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
MODEL_SIGNATURE_FILE := $(MODEL_OUT_PREFIX).signature.json
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
MODEL_IMPORT_ARGS = \
	--input $(MODEL_OBJ) \
	--out-prefix $(MODEL_OUT_PREFIX) \
	--symbol $(MODEL_SYMBOL) \
	--scale $(MODEL_SCALE) \
	--palette-index $(MODEL_PALETTE_INDEX) \
	--max-texture-width $(MODEL_MAX_TEXTURE_WIDTH) \
	--max-texture-height $(MODEL_MAX_TEXTURE_HEIGHT) \
	--texture-scale $(MODEL_TEXTURE_SCALE) \
	$(MODEL_FLIP_FLAGS)
$(MODEL_SIGNATURE_FILE): FORCE_MODEL_IMPORT_SIGNATURE $(MODEL_OBJ) $(MODEL_IMPORT_SRCS)
	@mkdir -p $(dir $@)
	$(PYTHON) $(TOOLS)/import_model.py $(MODEL_IMPORT_ARGS) \
		--signature-only --signature-file $@
$(MODEL_OUT_PREFIX).c $(MODEL_OUT_PREFIX).h $(MODEL_OUT_PREFIX).import.json &: $(MODEL_OBJ) $(MODEL_IMPORT_SRCS) $(MODEL_SIGNATURE_FILE)
	@mkdir -p $(dir $@)
	$(PYTHON) $(TOOLS)/import_model.py $(MODEL_IMPORT_ARGS) $(MODEL_IMPORT_INCREMENTAL_FLAG) $(MODEL_IMPORT_FORCE_FLAG)
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
	@rm -f $@
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

$(IP_GENERATED): $(BIN) $(IP_TEMPLATE) tools/gen_ip_bin.py tools/memory_layout.py
	$(PYTHON) tools/gen_ip_bin.py --template $(IP_TEMPLATE) --output $@ \
		--load-addr 0x$(APP_LOAD_ADDR_HEX) --first-read-file $(BIN)

$(ISO): $(BIN) $(IP_GENERATED) $(EXAMPLE_ISO_FILES)
	@rm -rf $(ISO_ROOT)
	@mkdir -p $(ISO_ROOT)
	@cp $(BIN) $(ISO_ROOT)/0.BIN
	@cp $(IP_GENERATED) $(ISO_ROOT)/IP.BIN
	@if [ -n "$(EXAMPLE_ISO_DIR)" ]; then \
		cp -R "$(EXAMPLE_ISO_DIR)/." "$(ISO_ROOT)/"; \
	fi
	$(MKISOFS) -quiet -sysid "SEGA SATURN" -volid "LIBSATURN" \
		-volset "LIBSATURN" -publisher "LIBSATURN" -preparer "LIBSATURN" \
		-A "LIBSATURN" -G $(IP_GENERATED) -full-iso9660-filenames \
		-o $@ $(ISO_ROOT)

# -- CUE --------------------------------------------------------
$(CUE): $(ISO)
	$(PYTHON) $(TOOLS)/gen_cue.py \
		--iso-name $(EXAMPLE).iso \
		--cue-output $(CUE)

# -- Host tests -------------------------------------------------
# tests/host/*.cpp exercise the pure helpers in src/core/runtime/logic.hpp. They are
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
HOST_TEST_EXTRA_test_font_logic := src/graphics/2d/font/api.cpp
HOST_TEST_EXTRA_test_font_text_logic :=
HOST_TEST_EXTRA_test_audio_stream_logic :=
HOST_TEST_EXTRA_test_audio_stream_api := src/audio/streaming/api.cpp src/audio/streaming/runtime.cpp
HOST_TEST_EXTRA_test_music_api := src/audio/playback/music.cpp src/audio/streaming/api.cpp src/audio/streaming/runtime.cpp
HOST_TEST_EXTRA_test_save_api := src/storage/save/api.cpp
HOST_TEST_EXTRA_test_save_schema := src/storage/save/schema.cpp
HOST_TEST_EXTRA_test_resource_plan := src/resources/plan.cpp
HOST_TEST_EXTRA_test_hud := src/graphics/2d/hud.cpp
HOST_TEST_EXTRA_test_sprite_anim := src/graphics/2d/sprites/animation.cpp
HOST_TEST_EXTRA_test_view_cache := src/graphics/3d/scene/view_cache.cpp src/graphics/3d/scene/view_cache_world.cpp
HOST_TEST_EXTRA_test_view_cache_isolation := src/graphics/3d/scene/view_cache.cpp
HOST_TEST_EXTRA_test_surface3d := src/graphics/3d/rendering/surface.cpp
HOST_TEST_EXTRA_test_vdp2_environment := src/graphics/vdp2/environment.cpp
HOST_TEST_EXTRA_test_ram_cart_api := src/storage/cartridge/api.cpp src/core/memory/api.cpp
HOST_TEST_EXTRA_test_ram_cart_1m := src/storage/cartridge/api.cpp src/core/memory/api.cpp
HOST_TEST_EXTRA_test_file_asset_logic := src/storage/files/api.cpp src/resources/assets.cpp src/storage/files/asset_runtime.cpp src/graphics/2d/textures/api.cpp src/core/runtime/state.cpp src/graphics/2d/palette/registry.cpp src/graphics/2d/textures/runtime.cpp
HOST_TEST_EXTRA_test_cdfs_logic := src/storage/cd/api.cpp src/storage/cd/filesystem.cpp src/storage/files/api.cpp src/storage/files/asset_runtime.cpp
HOST_TEST_EXTRA_test_cd_block_api := src/hal/cd/block.cpp src/storage/cd/api.cpp
HOST_TEST_EXTRA_test_math3d_logic := src/core/math3d/api.cpp
HOST_TEST_EXTRA_test_orbit_camera3d := src/graphics/3d/camera/orbit.cpp src/core/math3d/api.cpp
HOST_TEST_EXTRA_test_follow_camera3d := src/graphics/3d/camera/follow.cpp
HOST_TEST_EXTRA_test_transform3d := src/graphics/3d/geometry/transform.cpp src/graphics/3d/scene/api.cpp src/core/math3d/api.cpp
HOST_TEST_EXTRA_test_physics3_transform := src/physics/3d/transform.cpp src/physics/3d/world.cpp src/graphics/3d/geometry/transform.cpp src/graphics/3d/scene/api.cpp src/core/math3d/api.cpp src/physics/3d/collision.cpp src/physics/3d/sweep.cpp src/physics/3d/sweep_full.cpp
HOST_TEST_EXTRA_test_scene_transform3d := src/graphics/3d/scene/transform.cpp src/graphics/3d/geometry/transform.cpp src/graphics/3d/scene/api.cpp src/core/math3d/api.cpp
HOST_TEST_EXTRA_test_render3d_logic := src/core/math3d/api.cpp
HOST_TEST_EXTRA_test_voxel_terrain := src/physics/spatial/voxel_terrain.cpp src/core/math3d/api.cpp
HOST_TEST_EXTRA_test_voxel_display_probe :=
HOST_TEST_EXTRA_test_render3d_effects := src/graphics/3d/rendering/api.cpp src/core/math3d/api.cpp src/core/runtime/state.cpp
HOST_TEST_EXTRA_test_render3d_indexed := src/graphics/3d/rendering/indexed.cpp
HOST_TEST_EXTRA_test_vdp2_bitmap := src/graphics/vdp2/bitmap.cpp
HOST_TEST_EXTRA_test_mesh3d_logic := src/core/geometry/mesh_api.cpp src/core/math3d/api.cpp
HOST_TEST_EXTRA_test_geometry_link_isolation := src/core/geometry/mesh_api.cpp src/core/math3d/api.cpp
HOST_TEST_EXTRA_test_mesh3d_textured := src/graphics/3d/geometry/mesh.cpp src/core/geometry/mesh_api.cpp src/core/math3d/api.cpp
HOST_TEST_EXTRA_test_vdp1_upload := src/graphics/vdp1/api.cpp src/core/runtime/state.cpp src/graphics/2d/palette/registry.cpp
HOST_TEST_EXTRA_test_vdp1_clip := src/hal/vdp1/vdp1.cpp
HOST_TEST_EXTRA_test_texture_api := src/graphics/2d/textures/api.cpp src/core/runtime/state.cpp src/graphics/2d/palette/registry.cpp src/graphics/2d/textures/runtime.cpp
HOST_TEST_EXTRA_test_render2d_api := src/graphics/2d/rendering/api.cpp src/graphics/2d/rendering/runtime.cpp src/graphics/2d/textures/api.cpp src/graphics/2d/textures/runtime.cpp src/graphics/2d/palette/registry.cpp src/core/runtime/state.cpp
HOST_TEST_EXTRA_test_input_api := src/input/api.cpp src/input/runtime.cpp src/core/runtime/state.cpp
HOST_TEST_EXTRA_test_model3d_logic := src/graphics/3d/geometry/model.cpp src/graphics/3d/geometry/mesh.cpp src/core/geometry/mesh_api.cpp src/core/math3d/api.cpp
HOST_TEST_EXTRA_test_scene3d_api := src/graphics/3d/scene/api.cpp src/graphics/3d/geometry/model.cpp src/core/math3d/api.cpp
HOST_TEST_EXTRA_test_scene_api := src/graphics/3d/scene/scene.cpp src/core/math3d/api.cpp
HOST_TEST_EXTRA_test_scene_parallel_runtime := src/graphics/3d/scene/parallel.cpp
HOST_TEST_EXTRA_test_scene3d_faces_api := src/graphics/3d/scene/faces.cpp
HOST_TEST_EXTRA_test_scene3d_material_pool := src/graphics/3d/materials/pool.cpp
HOST_TEST_EXTRA_test_anim3d_logic := src/graphics/3d/animation/api.cpp src/graphics/3d/geometry/model.cpp src/graphics/3d/geometry/mesh.cpp src/core/geometry/mesh_api.cpp src/core/math3d/api.cpp
HOST_TEST_EXTRA_test_anim3d_async_lifetime := src/graphics/3d/animation/api.cpp src/graphics/3d/geometry/model.cpp src/graphics/3d/geometry/mesh.cpp src/core/geometry/mesh_api.cpp src/core/math3d/api.cpp
HOST_TEST_EXTRA_test_pacman_game := examples/common/pacman_game.c examples/common/pacman_level.c examples/common/pacman_stages.c src/physics/2d/grid.cpp src/physics/2d/collision.cpp src/core/math3d/api.cpp
HOST_TEST_EXTRA_test_skybridge_game := src/graphics/3d/rendering/surface.cpp
HOST_TEST_EXTRA_test_skybridge_fade := src/graphics/3d/rendering/surface.cpp
HOST_TEST_EXTRA_test_collide2d_logic := src/physics/2d/collision.cpp
HOST_TEST_EXTRA_test_spatial_logic := src/physics/spatial/2d.cpp src/physics/2d/collision.cpp
HOST_TEST_EXTRA_test_physics_logic := src/physics/2d/collision.cpp src/physics/2d/grid.cpp
HOST_TEST_EXTRA_test_physics3_world := src/physics/3d/world.cpp src/physics/3d/collision.cpp src/physics/3d/sweep.cpp src/physics/3d/sweep_full.cpp
HOST_TEST_EXTRA_test_collide3d_sweep := src/physics/3d/sweep.cpp
HOST_TEST_EXTRA_test_collide3d_sweep_full := src/physics/3d/sweep_full.cpp
HOST_TEST_EXTRA_test_collide3d_logic :=
HOST_TEST_EXTRA_test_spatial3_api := src/physics/spatial/3d.cpp src/physics/3d/collision.cpp

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
