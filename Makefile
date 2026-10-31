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

# The first rule below is an implementation detail; a bare `make` builds the example.
.DEFAULT_GOAL := all

# -- Toolchain --------------------------------------------------
TARGET      := sh2eb-elf
CC          := $(TARGET)-gcc
CXX         := $(TARGET)-g++
AR          := $(TARGET)-ar
OBJCOPY     := $(TARGET)-objcopy

# Python: prefer Windows one (has Pillow), fallback to MSYS2
PYTHON_WIN  := $(shell command -v python 2>/dev/null || command -v python3 2>/dev/null)
PYTHON_MSYS := $(shell command -v /usr/bin/python 2>/dev/null)
PYTHON      ?= $(PYTHON_WIN)

MKISOFS     := $(shell command -v mkisofs 2>/dev/null || command -v genisoimage 2>/dev/null || command -v xorrisofs 2>/dev/null)

# -- Paths ------------------------------------------------------
BUILD_DIR    := build
GENERATED_DIR := $(BUILD_DIR)/generated
TOOLS        := tools

# -- Config -----------------------------------------------------
EXAMPLE      ?= hello_world
SOURCE_DATE_EPOCH ?= 1791936000
export SOURCE_DATE_EPOCH
IP_PROFILE   ?= current
IP_TEMPLATE_KIND ?= native
APP_LOAD_ADDR_HEX := $(shell $(PYTHON) tools/memory_layout.py app_load_hex 2>/dev/null || echo 06004000)

# -- Per-example build lock ----------------------------------------
# Every build of one example shares build/generated/<example>/ (model import
# staging), and builds whose settings differ only in MODEL_* values share
# their variant directory and the build/<example>.* exports too. Two such
# builds running at once would overwrite each other's inputs mid-build. A
# plain `make EXAMPLE=x [all]` therefore re-runs itself under an flock on
# build/locks/<example>.lock: builds of one example serialize, different
# examples still build in parallel. LIBSATURN_EXAMPLE_LOCKED marks the
# locked run; without flock the build runs unlocked, as it always did.
LIBSATURN_LOCK_TRAMPOLINE :=
ifeq ($(LIBSATURN_EXAMPLE_LOCKED),)
ifneq ($(shell command -v flock 2>/dev/null),)
ifeq ($(filter-out all,$(or $(MAKECMDGOALS),all)),)
LIBSATURN_LOCK_TRAMPOLINE := 1
.PHONY: all
all:
	@mkdir -p $(BUILD_DIR)/locks && \
	LIBSATURN_EXAMPLE_LOCKED=1 flock $(BUILD_DIR)/locks/$(EXAMPLE).lock \
		$(MAKE) --no-print-directory all
endif
endif
endif

ifeq ($(LIBSATURN_LOCK_TRAMPOLINE),)
MAX_APP_BIN_BYTES := 983040

VALID_IP_PROFILES  := current safe
VALID_IP_TEMPLATE_KINDS := native yaul sbl minimal yaul_fixed region_free minimal_boot correct final

ifneq ($(filter $(IP_PROFILE),$(VALID_IP_PROFILES)),$(IP_PROFILE))
$(error Invalid IP_PROFILE '$(IP_PROFILE)'. Use: $(VALID_IP_PROFILES))
endif
ifneq ($(filter $(IP_TEMPLATE_KIND),$(VALID_IP_TEMPLATE_KINDS)),$(IP_TEMPLATE_KIND))
$(error Invalid IP_TEMPLATE_KIND '$(IP_TEMPLATE_KIND)'. Use: $(VALID_IP_TEMPLATE_KINDS))
endif

# -- IP template ------------------------------------------------
NATIVE_BOOT_PAYLOADS ?= $(BUILD_DIR)/native-boot/payloads.json
# Default manifest: local security/area inputs extracted from the tracked YAUL template.
# A user-supplied NATIVE_BOOT_PAYLOADS skips this rule.
ifeq ($(origin NATIVE_BOOT_PAYLOADS),file)
$(NATIVE_BOOT_PAYLOADS): assets/boot/ip_yaul_template.bin tools/prepare_native_payloads.py tools/boot_image.py
	$(PYTHON) tools/prepare_native_payloads.py --template assets/boot/ip_yaul_template.bin --out $(dir $@)
endif
IP_TEMPLATE := assets/boot/ip_$(IP_TEMPLATE_KIND)_template.bin
IP_GENERATED = $(OUTPUT_DIR)/$(EXAMPLE).IP.BIN

# -- Compilacao -------------------------------------------------
BASE_CFLAGS := -m2 -mb -O2 -ffreestanding -fomit-frame-pointer -Wall -Wextra \
               -ffunction-sections -fdata-sections \
               -Iinclude -I.
# Generated model/asset headers are example-local inputs, never part of
# generic library compilation or its cache key.
# examples/common holds example-only helpers (example_util.h); it is on the
# example include path only, never the library's or the public package's.
CFLAGS      := $(BASE_CFLAGS) -I$(GENERATED_DIR) -Iexamples/common $(EXAMPLE_INCFLAGS)
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

# Only generic switches affect reusable library objects. Game-specific
# defines never enter generic translation units, and profile changes select
# different object/archive paths instead of touching every .o on each build.
LIB_PROFILE_METRICS := 0
ifneq ($(filter skeletal_animation_3d fighter_arena_3d skinning_stress_3d killer_clown_demo,$(EXAMPLE)),)
LIB_PROFILE_METRICS := 1
endif
ifeq ($(EXAMPLE),skybridge_3d)
ifneq ($(strip $(SAT_SKYBRIDGE_VALIDATION)),)
LIB_PROFILE_METRICS := 1
endif
endif
ifeq ($(EXAMPLE),parallel_runtime)
ifneq ($(strip $(SAT_PARALLEL_RUNTIME_VALIDATION)),)
LIB_PROFILE_METRICS := 1
endif
endif
ifeq ($(EXAMPLE):$(DINO_PIPELINE_METRICS),dino_demo:1)
LIB_PROFILE_METRICS := 1
endif
ifneq ($(strip $(SAT_PROFILE_METRICS)),)
LIB_PROFILE_METRICS := 1
endif
LIB_FAULT_MODE := $(if $(strip $(SAT_PARALLEL_TEST_FAULT)),$(SAT_PARALLEL_TEST_FAULT),0)
SAT_LEGACY_EARLY_SCU ?= 0
LIB_CFLAGS := $(BASE_CFLAGS) -DSAT_LEGACY_EARLY_SCU=$(SAT_LEGACY_EARLY_SCU) -DSAT_PROFILE_METRICS=$(LIB_PROFILE_METRICS) \
              -DSAT_PARALLEL_TEST_FAULT=$(LIB_FAULT_MODE)
LIB_CXXFLAGS := $(LIB_CFLAGS) -std=c++20 -fno-exceptions -fno-rtti \
                -fno-threadsafe-statics -fno-use-cxa-atexit
LIB_PROFILE_KEY := $(shell $(PYTHON) tools/build_variant_key.py \
    "$(LIB_CFLAGS)" "$(LIB_CXXFLAGS)" "$(ASFLAGS)" "scene_faces=-O3")
LIB_OBJ_ROOT := $(BUILD_DIR)/objects/library/$(LIB_PROFILE_KEY)

# -- Biblioteca -------------------------------------------------
# The runtime source inventory is cmake/LibSaturnSources.cmake, the same
# manifest the CMake package consumes; tools/library_sources.py reads it, so
# adding a runtime source means editing that one file. Startup assembly is a
# separate list because it is linked as loose objects, never archived.
LIB_SOURCE_LIST := $(shell $(PYTHON) tools/library_sources.py)
ifeq ($(strip $(LIB_SOURCE_LIST)),)
$(error tools/library_sources.py returned no sources from cmake/LibSaturnSources.cmake)
endif
LIB_CPP_SRCS := $(filter %.cpp,$(LIB_SOURCE_LIST))
LIB_C_SRCS   := $(filter %.c,$(LIB_SOURCE_LIST))
CRT_SRCS     := $(shell $(PYTHON) tools/library_sources.py --startup)

LIB_CPP_OBJS := $(patsubst %.cpp,$(LIB_OBJ_ROOT)/%.o,$(LIB_CPP_SRCS))
LIB_C_OBJS   := $(patsubst %.c,$(LIB_OBJ_ROOT)/%.o,$(LIB_C_SRCS))
LIBRARY := $(LIB_OBJ_ROOT)/libsaturn.a

# -- Example (automatic discovery) ----------------------------
# Each example can have a Makefile.inc defining:
#   EXAMPLE_ASSETS  = list of assets (e.g. $(GENERATED_DIR)/sega_bg/bg.c)
#   EXAMPLE_HEADERS = generated headers
#   EXAMPLE_ISO_DIR = directory whose contents are copied into the ISO root
#   EXAMPLE_ISO_FILES = generated/checked-in files required by that directory
#   EXAMPLE_AUDIO_STREAMS = names of sources converted to CD-backed audio
#   EXAMPLE_DEPS    = dependencies for generation
#   EXAMPLE_RESIZE  = width height (e.g. 320 224)
#   EXAMPLE_INPUT   = original image path
# ---------------------------------------------------------------
EXAMPLE_DIR     := examples/$(EXAMPLE)
EXAMPLE_HEADERS :=

# Additional sources outside an example's own directory are opt-in.
# This is for family-owned code shared by multiple runnable examples, e.g.
# pacman_2d and pacman_3d both consume examples/pacman/.
EXAMPLE_EXTRA_SRCS :=

# Include example config if it exists
EXAMPLE_INC := $(EXAMPLE_DIR)/Makefile.inc
ifneq ($(wildcard $(EXAMPLE_INC)),)
  include $(EXAMPLE_INC)
endif

# Per-example include paths are declared in Makefile.inc, so they have to be
# layered on top of the base CFLAGS after the include above.
CFLAGS += $(EXAMPLE_INCFLAGS)

# Optional generic audio asset pipeline. An example declares each stream's
# source, ISO-relative path, sample rate and channels; the normal build then
# converts it with FFmpeg, emits metadata for C, and stages it in the ISO.
FFMPEG ?= ffmpeg
ifneq ($(strip $(EXAMPLE_AUDIO_STREAMS)),)
define AUDIO_STREAM_RULE
AUDIO_$(1)_FILE := $(GENERATED_DIR)/$(EXAMPLE)/audio/$(EXAMPLE_AUDIO_$(1)_DISC_PATH)
AUDIO_$(1)_HEADER := $(GENERATED_DIR)/$(EXAMPLE)/audio_$(1).h
EXAMPLE_HEADERS += $$(AUDIO_$(1)_HEADER)
EXAMPLE_ISO_FILES += $$(AUDIO_$(1)_FILE)
$$(AUDIO_$(1)_FILE) $$(AUDIO_$(1)_HEADER) &: $(EXAMPLE_AUDIO_$(1)_SOURCE) tools/convert_audio.py
	@mkdir -p $$(dir $$(AUDIO_$(1)_FILE)) $$(dir $$(AUDIO_$(1)_HEADER))
	$$(PYTHON) tools/convert_audio.py --input "$(EXAMPLE_AUDIO_$(1)_SOURCE)" \
		--output-audio "$$(AUDIO_$(1)_FILE)" --output-header "$$(AUDIO_$(1)_HEADER)" \
		--name "$(1)" --sample-rate "$(EXAMPLE_AUDIO_$(1)_SAMPLE_RATE)" \
		--channels "$(EXAMPLE_AUDIO_$(1)_CHANNELS)" \
		--format "$(or $(EXAMPLE_AUDIO_$(1)_FORMAT),s16be)" \
		--start-ms "$(or $(EXAMPLE_AUDIO_$(1)_START_MS),0)" \
		--duration-ms "$(or $(EXAMPLE_AUDIO_$(1)_DURATION_MS),0)" \
		--loop-crossfade-ms "$(or $(EXAMPLE_AUDIO_$(1)_LOOP_CROSSFADE_MS),0)" \
		--ffmpeg "$(FFMPEG)"
endef
$(foreach audio,$(EXAMPLE_AUDIO_STREAMS),$(eval $(call AUDIO_STREAM_RULE,$(audio))))
endif

ifneq ($(strip $(EXAMPLE_CDDA_INPUT)),)
EXAMPLE_CDDA_SOURCE := $(GENERATED_DIR)/$(EXAMPLE)/audio/cdda.pcm
$(EXAMPLE_CDDA_SOURCE): $(EXAMPLE_CDDA_INPUT) tools/convert_cd_audio.py
	$(PYTHON) tools/convert_cd_audio.py --input "$(EXAMPLE_CDDA_INPUT)" \
		--output "$@" --ffmpeg "$(FFMPEG)"
endif

# Example objects and outputs are keyed after the include, so flags an
# example's Makefile.inc adds (dino_demo's -DDINO_MUSIC_68K) select their own
# object directory instead of reusing objects built without them.
EXAMPLE_PROFILE_KEY := $(shell $(PYTHON) tools/build_variant_key.py \
    "$(EXAMPLE)" "$(IP_PROFILE)" "$(IP_TEMPLATE_KIND)" "$(CFLAGS)" \
    "$(CXXFLAGS)" "$(ASFLAGS)" "$(LIB_PROFILE_KEY)")
APP_OBJ_ROOT := $(BUILD_DIR)/objects/examples/$(EXAMPLE)/$(EXAMPLE_PROFILE_KEY)
OUTPUT_DIR := $(BUILD_DIR)/variants/$(EXAMPLE)/$(EXAMPLE_PROFILE_KEY)
ISO_ROOT := $(OUTPUT_DIR)/iso_root
LDFLAGS := -m2 -mb -nostdlib -Wl,-T,src/core/startup/saturn.ld \
           -Wl,-Map,$(OUTPUT_DIR)/$(EXAMPLE).map -Wl,--gc-sections
CRT_OBJS     := $(patsubst %.s,$(APP_OBJ_ROOT)/%.o,$(CRT_SRCS))

# Computed after the include so Makefile.inc can contribute to it.
EXAMPLE_SRCS    := $(wildcard $(EXAMPLE_DIR)/*.c) $(EXAMPLE_EXTRA_SRCS)
EXAMPLE_OBJS    := $(patsubst %.c,$(APP_OBJ_ROOT)/%.o,$(EXAMPLE_SRCS))
EXAMPLE_ASSET_OBJS := $(patsubst %.c,$(APP_OBJ_ROOT)/%.o,$(EXAMPLE_ASSETS))

ALL_APP_OBJS := $(EXAMPLE_OBJS) $(EXAMPLE_ASSET_OBJS)
ALL_HEADERS  := $(EXAMPLE_HEADERS)

# Pulls in the per-object dependency files DEPFLAGS (-MMD -MP) writes, so a
# header edit rebuilds every object that #includes it -- library, example and
# generated-asset alike -- not just the one .c/.cpp file whose own mtime
# changed. Without this, `-MMD -MP` writes the .d files but nothing ever
# reads them, which is worse than not having them at all: it looks like
# header dependencies are tracked when they are not. A stale object from a
# shared header (examples/pacman/pacman_game.h, in the case that found this)
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

# Game profiles are isolated by object/artifact paths rather than phony
# prerequisites that force recompilation of unchanged library code.

# -- Artefatos --------------------------------------------------
ELF := $(OUTPUT_DIR)/$(EXAMPLE).elf
APP_BIN := $(OUTPUT_DIR)/$(EXAMPLE).app.bin
ISO := $(OUTPUT_DIR)/$(EXAMPLE).iso
DISC_BIN := $(OUTPUT_DIR)/$(EXAMPLE).bin
CUE := $(OUTPUT_DIR)/$(EXAMPLE).cue

# -- Available examples ---------------------------------------
EXAMPLES := $(patsubst examples/%/main.c,%,$(wildcard examples/*/main.c))

.PHONY: all clean dirs check-tools examples-all list-examples bake test

all: check-tools dirs $(ELF) $(DISC_BIN) $(CUE) $(LIBRARY)
	@# Compatibility exports are never inputs to a cached variant's build.
	@cp $(ELF) $(BUILD_DIR)/$(EXAMPLE).elf
	@cp $(APP_BIN) $(BUILD_DIR)/$(EXAMPLE).app.bin
	@cp $(ISO) $(BUILD_DIR)/$(EXAMPLE).iso
	@cp $(IP_GENERATED) $(BUILD_DIR)/$(EXAMPLE).IP.BIN
	@cp $(DISC_BIN) $(BUILD_DIR)/$(EXAMPLE).bin
	@cp $(CUE) $(BUILD_DIR)/$(EXAMPLE).cue
	@# The harness reads symbol addresses (e.g. Skybridge telemetry) here.
	@cp $(OUTPUT_DIR)/$(EXAMPLE).map $(BUILD_DIR)/$(EXAMPLE).map

# -- Verificacoes -----------------------------------------------
check-tools:
	@if ! command -v $(CC) >/dev/null 2>&1; then \
		echo "Error: $(CC) not found in PATH"; exit 1; fi
	@if [ -z "$(MKISOFS)" ]; then \
		echo "Error: mkisofs/genisoimage/xorrisofs not found"; exit 1; fi
	@if [ -n "$(strip $(EXAMPLE_AUDIO_STREAMS) $(EXAMPLE_CDDA_INPUT))" ] && ! $(PYTHON) -c 'import shutil,sys; sys.exit(0 if shutil.which(sys.argv[1]) else 1)' "$(FFMPEG)"; then \
		echo "Error: FFmpeg not found (set FFMPEG=/path/to/ffmpeg)"; exit 1; fi
	@echo "[profiles] EXAMPLE=$(EXAMPLE) IP_PROFILE=$(IP_PROFILE) IP_TEMPLATE=$(IP_TEMPLATE_KIND)"

dirs:
	@mkdir -p $(BUILD_DIR) $(GENERATED_DIR) $(ISO_ROOT) $(OUTPUT_DIR)
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
CHARACTER_EXAMPLE_GEN := $(GENERATED_DIR)/character_examples
CHARACTER_EXAMPLE_OUTPUTS := $(addprefix $(CHARACTER_EXAMPLE_GEN)/,fighter_high.c fighter_medium.c fighter_low.c fighter_assets.c fighter_high.h fighter_medium.h fighter_low.h fighter_assets.h report.json) $(addprefix $(CHARACTER_EXAMPLE_GEN)/disc/,high_blue.ska high_red.ska medium_blue.ska medium_red.ska low_blue.ska low_red.ska)
$(CHARACTER_EXAMPLE_OUTPUTS) &: tools/generate_character_examples.py tools/model_pipeline/skeletal_binary.py tools/model_pipeline/skeletal_export.py tools/model_pipeline/skeletal_textures.py tools/model_pipeline/model.py tools/model_pipeline/animation.py
	$(PYTHON) tools/generate_character_examples.py --output $(CHARACTER_EXAMPLE_GEN)

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
# MODEL_SAMPLING (nearest|area) and MODEL_TEXEL_EXTENT (texels across the
# model's longest extent, empty for none) bake each face at no more texels
# than it covers on screen, box-filtered: the VDP1 has no mipmapping.
# MODEL_LUT_CODES (LO-HI, empty for RGB): lut4 tables hold codes LO..HI of
# one shared palette, for the 8 bits/pixel hi-res framebuffer.
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
MODEL_TEXTURE_LIGHTING    ?= off
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
MODEL_SAMPLING              ?= nearest
MODEL_TEXEL_EXTENT          ?=
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
MODEL_TARGET             ?= saturn
MODEL_LOOP_CLIPS         ?=
MODEL_MAX_TRIANGLES      ?=
MODEL_IMPORT_ARGS = \
	--input $(MODEL_GLB) \
	--target $(MODEL_TARGET) \
	--out-prefix $(MODEL_OUT_PREFIX) \
	--symbol $(MODEL_SYMBOL) \
	--scale $(MODEL_SCALE) \
	$(if $(MODEL_LOOP_CLIPS),--loop-clips $(MODEL_LOOP_CLIPS)) \
	$(if $(MODEL_MAX_TRIANGLES),--max-triangles $(MODEL_MAX_TRIANGLES)) \
	$(if $(filter 1,$(MODEL_SKELETAL_SURFACE_BAKE)),--skeletal-surface-bake) \
	$(if $(MODEL_SKELETAL_EXPOSURE),--skeletal-exposure $(MODEL_SKELETAL_EXPOSURE)) \
	$(if $(filter 1,$(MODEL_SKELETAL_STUDIO_LIGHT)),--skeletal-studio-light) \
	--palette-index $(MODEL_PALETTE_INDEX) \
	--max-texture-width $(MODEL_MAX_TEXTURE_WIDTH) \
	--max-texture-height $(MODEL_MAX_TEXTURE_HEIGHT) \
	--texture-scale $(MODEL_TEXTURE_SCALE) \
	--simplify $(MODEL_SIMPLIFY) \
	--quality $(MODEL_QUALITY) \
	--animation $(MODEL_ANIMATION) \
	--animation-fps $(MODEL_ANIMATION_FPS) \
	$(if $(filter skeletal,$(MODEL_TARGET)),--texture-format $(MODEL_TEXTURE_FORMAT)) \
	$(if $(filter skeletal,$(MODEL_TARGET)),, \
	--face-colors $(MODEL_FACE_COLORS) \
	--texture-lighting $(MODEL_TEXTURE_LIGHTING) \
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
	$(if $(MODEL_LUT_CODES),--lut-codes $(MODEL_LUT_CODES))) \
	--sampling $(MODEL_SAMPLING) \
	$(if $(MODEL_TEXEL_EXTENT),--texel-extent $(MODEL_TEXEL_EXTENT)) \
	--report $(MODEL_OUT_PREFIX).report.json \
	$(MODEL_FLIP_FLAGS) $(MODEL_LOD_FLAG) $(MODEL_MERGE_RIGID_MESHES_FLAG)
# The incremental cache always hashes a small set of output roles; ensure
# every role the cache expects is materialised, even when the skeletal
# target skips baked-pose artefacts.
_MODEL_SKELETAL_OUTPUTS := $(MODEL_OUT_PREFIX).h $(MODEL_OUT_PREFIX).c
_$(MODEL_TARGET)_DUMMY_ASSETS ?=
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
$(LIB_OBJ_ROOT)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(LIB_CFLAGS) $(DEPFLAGS) -c $< -o $@

$(LIB_OBJ_ROOT)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(LIB_CXXFLAGS) $(if $(filter src/graphics/3d/scene/faces.cpp,$<),-O3) $(DEPFLAGS) -c $< -o $@

$(APP_OBJ_ROOT)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

$(APP_OBJ_ROOT)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

$(APP_OBJ_ROOT)/%.o: %.s
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

# -- Linkagem ---------------------------------------------------
$(LIBRARY): $(LIB_CPP_OBJS) $(LIB_C_OBJS)
	@mkdir -p $(dir $@)
	@rm -f $@
	$(AR) rcs $@ $^

$(ELF): $(CRT_OBJS) $(ALL_APP_OBJS) $(LIBRARY)
	@mkdir -p $(dir $@)
	$(CXX) $(LDFLAGS) -o $@ $(CRT_OBJS) $(ALL_APP_OBJS) -L$(LIB_OBJ_ROOT) -lsaturn -lgcc
	@# crt0 never runs static constructors, so a global needing one is left
	@# null and fails in ways that look like a hardware bug. Catch it here.
	$(PYTHON) $(TOOLS)/check_no_init_array.py $@

$(APP_BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@
	@size=$$(wc -c < $@); \
	if [ $$size -gt $(MAX_APP_BIN_BYTES) ]; then \
		echo "Error: $(APP_BIN) is $$size bytes; maximum is $(MAX_APP_BIN_BYTES) bytes"; \
		exit 1; \
	fi

ifeq ($(IP_TEMPLATE_KIND),native)
$(IP_GENERATED): $(APP_BIN) $(ELF) src/core/startup/aip.s tools/check_boot_memory.py tools/boot_trace_address.py $(NATIVE_BOOT_PAYLOADS) $(wildcard $(dir $(NATIVE_BOOT_PAYLOADS))*.bin) tools/build_native_ip.py tools/check_native_ip.py tools/boot_image.py
	@mkdir -p $(dir $@)
	$(PYTHON) tools/check_boot_memory.py --elf $(ELF) --app $(APP_BIN)
	$(CC) $(ASFLAGS) -Wa,--defsym,AIP_TRACE_ADDRESS=$$($(PYTHON) tools/boot_trace_address.py --nm $(TARGET)-nm --elf $(ELF)) -c src/core/startup/aip.s -o $(OUTPUT_DIR)/aip.o
	$(OBJCOPY) -O binary -j .text.aip $(OUTPUT_DIR)/aip.o $(OUTPUT_DIR)/aip.bin
	$(PYTHON) tools/build_native_ip.py --payloads $(NATIVE_BOOT_PAYLOADS) --aip $(OUTPUT_DIR)/aip.bin --app $(APP_BIN) --profile $(IP_PROFILE) --output $@ --title LIBSATURN
	$(PYTHON) tools/check_native_ip.py --ip $@ --app $(APP_BIN) --payloads $(NATIVE_BOOT_PAYLOADS)
else
$(IP_GENERATED): $(APP_BIN) $(IP_TEMPLATE) tools/gen_ip_bin.py tools/check_ip_bin.py tools/memory_layout.py
	@mkdir -p $(dir $@)
	$(PYTHON) tools/gen_ip_bin.py --template $(IP_TEMPLATE) --output $@ \
		--profile $(IP_PROFILE) --load-addr 0x$(APP_LOAD_ADDR_HEX) --first-read-file $(APP_BIN)
	$(PYTHON) tools/check_ip_bin.py --ip-bin $@ --template $(IP_TEMPLATE) \
		--expected-size $$(wc -c < $(APP_BIN)) --profile $(IP_PROFILE) \
		--load-addr 0x$(APP_LOAD_ADDR_HEX)

endif

$(ISO): $(APP_BIN) $(IP_GENERATED) $(EXAMPLE_ISO_FILES) tools/check_iso.py tools/boot_image.py tools/normalize_iso_dates.py
	@mkdir -p $(dir $@)
	@rm -rf $(ISO_ROOT)
	@mkdir -p $(ISO_ROOT)
	@cp $(APP_BIN) $(ISO_ROOT)/0.BIN
	@cp $(IP_GENERATED) $(ISO_ROOT)/IP.BIN
	@if [ -n "$(EXAMPLE_ISO_DIR)" ]; then \
		cp -R "$(EXAMPLE_ISO_DIR)/." "$(ISO_ROOT)/"; \
	fi
ifneq ($(strip $(EXAMPLE_AUDIO_STREAMS)),)
	@$(foreach audio,$(EXAMPLE_AUDIO_STREAMS),mkdir -p "$(dir $(ISO_ROOT)/$(EXAMPLE_AUDIO_$(audio)_DISC_PATH))" || exit $$?;)
	@$(foreach audio,$(EXAMPLE_AUDIO_STREAMS),cp "$(AUDIO_$(audio)_FILE)" "$(ISO_ROOT)/$(EXAMPLE_AUDIO_$(audio)_DISC_PATH)" || exit $$?;)
endif
	$(MKISOFS) -quiet -sysid "SEGA SATURN" -volid "LIBSATURN" \
		-volset "LIBSATURN" -publisher "LIBSATURN" -preparer "LIBSATURN" \
		-A "LIBSATURN" -G $(IP_GENERATED) -full-iso9660-filenames \
		-o $@ $(ISO_ROOT)
	$(PYTHON) tools/normalize_iso_dates.py --iso $@ --epoch $(SOURCE_DATE_EPOCH)
	$(PYTHON) tools/check_iso.py --iso $@ --app $(APP_BIN) --expected-size $$(wc -c < $(APP_BIN)) \
		--profile $(IP_PROFILE) --load-addr 0x$(APP_LOAD_ADDR_HEX)

# The ISO is a useful intermediate for filesystem inspection and the harness,
# but the public BIN/CUE pair is the raw Mode 1 image expected by ODEs.
ifneq ($(strip $(EXAMPLE_CDDA_SOURCE)),)
# A CD-DA example publishes one mixed-mode BIN: data, audio pregap, exact PCM.
$(DISC_BIN) $(CUE) &: $(ISO) $(EXAMPLE_CDDA_SOURCE) tools/build_cd_audio.py tools/iso_to_raw.py
	$(PYTHON) tools/build_cd_audio.py --iso $(ISO) --audio "$(EXAMPLE_CDDA_SOURCE)" \
		--output-bin $(DISC_BIN) --output-cue $(CUE)
else
$(DISC_BIN): $(ISO) tools/iso_to_raw.py
	$(PYTHON) tools/iso_to_raw.py --iso $(ISO) --output $@

# -- CUE --------------------------------------------------------
$(CUE): $(DISC_BIN) tools/gen_cue.py tools/check_disc_image.py tools/iso_to_raw.py
	$(PYTHON) $(TOOLS)/gen_cue.py \
		--bin-name $(EXAMPLE).bin \
		--cue-output $(CUE)
	$(PYTHON) tools/check_disc_image.py --iso $(ISO) --bin $(DISC_BIN) --cue $(CUE)
endif

# -- Host tests -------------------------------------------------
# tests/host/*.cpp exercise the pure helpers in src/core/runtime/logic.hpp. They are
# built with the NATIVE compiler (not sh2eb-elf-g++) and run on the host, so
# they need no Saturn hardware, emulator or BIOS.
#   make test
HOST_CXX       ?= g++
# Root is for quoted project paths ("src/..."), not standard headers. On a
# case-insensitive macOS volume VERSION otherwise shadows C++20 <version>.
HOST_CXXFLAGS  := -std=c++20 -Wall -Wextra -O1 -Iinclude -iquote . -Ibuild/generated -Iexamples/common
HOST_TEST_SRCS := $(wildcard tests/host/*.cpp)
HOST_TEST_BINS := $(patsubst tests/host/%.cpp,$(BUILD_DIR)/tests/%,$(HOST_TEST_SRCS))
HOST_TOOL_TESTS := $(wildcard tests/tools/*.py)

# Some host tests exercise logic that lives in a library .cpp file rather than
# a header (e.g. font glyph tables), and stub out that file's hardware calls
# themselves (see the extern "C" stubs at the top of test_font_logic.cpp).
# List such extra sources per test name here.
HOST_TEST_EXTRA_test_font_logic := src/graphics/2d/font/api.cpp
HOST_TEST_EXTRA_test_core_logic := src/core/math3d/api.cpp
HOST_TEST_EXTRA_test_vdp2_allocator := src/hal/vdp2/allocator.cpp
HOST_TEST_EXTRA_test_vdp2_palette_alloc := src/hal/vdp2/allocator.cpp
HOST_TEST_EXTRA_test_vdp2_plan_api := src/hal/vdp2/plan.cpp src/hal/vdp2/allocator.cpp
HOST_TEST_EXTRA_test_font_text_logic :=
HOST_TEST_EXTRA_test_audio_voice_transaction := src/audio/playback/state.cpp src/audio/playback/voice.cpp
HOST_TEST_EXTRA_test_audio_stream_logic :=
HOST_TEST_EXTRA_test_audio_stream_api := src/audio/streaming/api.cpp src/audio/streaming/runtime.cpp
HOST_TEST_EXTRA_test_music_api := src/audio/playback/music.cpp src/audio/streaming/api.cpp src/audio/streaming/runtime.cpp
HOST_TEST_EXTRA_test_save_api := src/storage/save/api.cpp
HOST_TEST_EXTRA_test_save_schema := src/storage/save/schema.cpp
HOST_TEST_EXTRA_test_resource_plan := src/resources/plan.cpp
HOST_TEST_EXTRA_test_hud := src/graphics/2d/hud.cpp
HOST_TEST_EXTRA_test_sprite_anim := src/graphics/2d/sprites/animation.cpp
HOST_TEST_EXTRA_test_sprite_clip := src/graphics/2d/sprites/clip.cpp
HOST_TEST_EXTRA_test_task := src/core/task/task.cpp
HOST_TEST_EXTRA_test_stage2d_generated := src/physics/2d/terrain2.cpp src/physics/2d/path2.cpp src/core/math2d/api.cpp src/physics/spatial/entity_stream2.cpp src/graphics/2d/sprites/clip.cpp
HOST_TEST_EXTRA_test_view_cache := src/graphics/3d/scene/view_cache.cpp src/graphics/3d/scene/view_cache_world.cpp
HOST_TEST_EXTRA_test_view_cache_isolation := src/graphics/3d/scene/view_cache.cpp
HOST_TEST_EXTRA_test_surface3d := src/graphics/3d/rendering/surface.cpp
HOST_TEST_EXTRA_test_vdp2_environment := src/graphics/vdp2/environment.cpp
HOST_TEST_EXTRA_test_vdp2_floor := src/graphics/vdp2/floor.cpp src/core/math3d/api.cpp
HOST_TEST_EXTRA_test_ram_cart_api := src/storage/cartridge/api.cpp src/core/memory/api.cpp
HOST_TEST_EXTRA_test_ram_cart_1m := src/storage/cartridge/api.cpp src/core/memory/api.cpp
HOST_TEST_EXTRA_test_abus_api := src/storage/cartridge/abus.cpp
HOST_TEST_EXTRA_test_scu_dsp_api := src/core/runtime/scu_dsp.cpp
HOST_TEST_EXTRA_test_scsp_dsp_api := src/audio/playback/state.cpp src/audio/effects/api.cpp
HOST_TEST_EXTRA_test_sampler_api := src/audio/playback/state.cpp src/audio/sampler/api.cpp
HOST_TEST_EXTRA_test_mpeg_api := src/hal/cd/mpeg.cpp
HOST_TEST_EXTRA_test_uart_api := src/hal/comm/uart.cpp
HOST_TEST_EXTRA_test_file_asset_logic := src/storage/files/api.cpp src/resources/assets.cpp src/storage/files/asset_runtime.cpp src/storage/files/asset_cache.cpp src/graphics/2d/textures/api.cpp src/core/runtime/state.cpp src/graphics/2d/palette/registry.cpp src/graphics/2d/textures/runtime.cpp
HOST_TEST_EXTRA_test_loader := src/storage/cd/api.cpp src/storage/cd/filesystem.cpp src/storage/cd/loader.cpp src/storage/compression/lz.cpp src/graphics/2d/loading_screen.cpp src/storage/files/api.cpp src/storage/files/asset_runtime.cpp
HOST_TEST_EXTRA_test_cart_overlay := src/storage/cartridge/overlay.cpp src/storage/cd/api.cpp src/storage/cd/filesystem.cpp src/storage/cd/loader.cpp src/storage/compression/lz.cpp src/storage/files/api.cpp src/storage/files/asset_runtime.cpp
HOST_TEST_EXTRA_test_lz := src/storage/compression/lz.cpp
HOST_TEST_EXTRA_test_exec := src/storage/cd/exec.cpp src/storage/cd/api.cpp src/storage/cd/filesystem.cpp src/storage/files/api.cpp src/storage/files/asset_runtime.cpp
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
HOST_TEST_EXTRA_test_clown_lighting := examples/killer_clown_demo/lighting.c src/core/math3d/api.cpp
HOST_TEST_EXTRA_test_geometry_link_isolation := src/core/geometry/mesh_api.cpp src/core/math3d/api.cpp
HOST_TEST_EXTRA_test_mesh3d_textured := src/graphics/3d/geometry/mesh.cpp src/core/geometry/mesh_api.cpp src/core/math3d/api.cpp
HOST_TEST_EXTRA_test_vdp1_upload := src/graphics/vdp1/api.cpp src/core/runtime/state.cpp src/graphics/2d/palette/registry.cpp
HOST_TEST_EXTRA_test_vdp1_clip := src/hal/vdp1/vdp1.cpp tests/host_stubs/stub_scu_dma.cpp tests/host_stubs/stub_vdp1_clock.cpp
HOST_TEST_EXTRA_test_sprite_batch := src/hal/vdp1/vdp1.cpp src/graphics/2d/sprites/batch.cpp tests/host_stubs/stub_scu_dma.cpp tests/host_stubs/stub_vdp1_clock.cpp
HOST_TEST_EXTRA_test_sprite_atlas := src/graphics/2d/sprites/atlas.cpp src/graphics/2d/sprites/batch.cpp src/hal/vdp1/vdp1.cpp src/resources/plan.cpp tests/host_stubs/stub_scu_dma.cpp tests/host_stubs/stub_vdp1_clock.cpp
HOST_TEST_EXTRA_test_tate := src/graphics/2d/tate.cpp
HOST_TEST_EXTRA_test_disc_stream := src/storage/cd/disc_stream.cpp
HOST_TEST_EXTRA_test_music_disc := src/audio/playback/music.cpp src/audio/streaming/api.cpp src/audio/streaming/runtime.cpp src/storage/cd/disc_stream.cpp
HOST_TEST_EXTRA_test_bh_sim := examples/bullet_hell_stress/bh_sim.c src/physics/2d/collision.cpp src/physics/spatial/2d.cpp src/graphics/2d/tate.cpp
HOST_TEST_EXTRA_test_vdp1_checkpoint := src/hal/vdp1/vdp1.cpp tests/host_stubs/stub_scu_dma.cpp tests/host_stubs/stub_vdp1_clock.cpp
build/tests/test_vdp1_checkpoint: HOST_CXXFLAGS += -DSAT_NATIVE_SCENE_COMMANDS=1
HOST_TEST_EXTRA_test_vdp1_vram_region := src/hal/vdp1/vdp1.cpp tests/host_stubs/stub_scu_dma.cpp tests/host_stubs/stub_vdp1_clock.cpp
HOST_TEST_EXTRA_test_texture_api := src/graphics/2d/textures/api.cpp src/core/runtime/state.cpp src/graphics/2d/palette/registry.cpp src/graphics/2d/textures/runtime.cpp
HOST_TEST_EXTRA_test_render2d_api := src/graphics/2d/rendering/api.cpp src/graphics/2d/rendering/runtime.cpp src/graphics/2d/textures/api.cpp src/graphics/2d/textures/runtime.cpp src/graphics/2d/palette/api.cpp src/graphics/2d/palette/registry.cpp src/core/runtime/state.cpp
HOST_TEST_EXTRA_test_vdp1_color_calc_api := src/graphics/vdp1/color_calc.cpp
HOST_TEST_EXTRA_test_input_api := src/input/api.cpp src/input/runtime.cpp src/core/runtime/state.cpp
HOST_TEST_EXTRA_test_irq_api := src/core/runtime/irq.cpp src/core/runtime/state.cpp
HOST_TEST_EXTRA_test_dma_api := src/core/runtime/dma.cpp src/core/runtime/state.cpp
HOST_TEST_EXTRA_test_smpc_api := src/core/runtime/smpc.cpp src/core/runtime/state.cpp
HOST_TEST_EXTRA_test_model3d_logic := src/graphics/3d/geometry/model.cpp src/graphics/3d/geometry/model_upload.cpp src/graphics/3d/geometry/mesh.cpp src/core/geometry/mesh_api.cpp src/core/math3d/api.cpp
HOST_TEST_EXTRA_test_scene3d_api := src/graphics/3d/scene/api.cpp src/graphics/3d/geometry/model.cpp src/core/math3d/api.cpp
HOST_TEST_EXTRA_test_scene_api := src/graphics/3d/scene/scene.cpp src/core/math3d/api.cpp
HOST_TEST_EXTRA_test_scene_managed_texture := src/graphics/3d/scene/managed_texture.cpp
HOST_TEST_EXTRA_test_parallel_completion := src/core/parallel/executor.cpp
HOST_TEST_EXTRA_test_scene_parallel_runtime := src/graphics/3d/scene/parallel.cpp
HOST_TEST_EXTRA_test_scene3d_faces_api := src/graphics/3d/scene/faces.cpp
HOST_TEST_EXTRA_test_scene_native_commands := src/graphics/3d/scene/faces.cpp src/hal/vdp1/vdp1.cpp tests/host_stubs/stub_scu_dma.cpp tests/host_stubs/stub_vdp1_clock.cpp
build/tests/test_scene_native_commands: HOST_CXXFLAGS += -DSAT_NATIVE_SCENE_COMMANDS=1
HOST_TEST_EXTRA_test_scene3d_material_pool := src/graphics/3d/materials/pool.cpp
HOST_TEST_EXTRA_test_skeleton3d := src/graphics/3d/animation/skeleton.cpp src/graphics/3d/animation/clip.cpp src/graphics/3d/geometry/transform.cpp src/graphics/3d/scene/api.cpp src/core/math3d/api.cpp
HOST_TEST_EXTRA_test_skeleton_clip := $(HOST_TEST_EXTRA_test_skeleton3d)
HOST_TEST_EXTRA_test_skin3d := $(HOST_TEST_EXTRA_test_skeleton3d) src/graphics/3d/animation/skin.cpp
HOST_TEST_EXTRA_test_skin_scene := src/graphics/3d/scene/faces.cpp src/graphics/3d/animation/skin.cpp
HOST_TEST_EXTRA_test_skin_present := src/graphics/3d/animation/character.cpp $(HOST_TEST_EXTRA_test_skin3d)
HOST_TEST_EXTRA_test_character_asset := src/graphics/3d/animation/character_asset.cpp $(HOST_TEST_EXTRA_test_skin3d)
HOST_TEST_EXTRA_test_character_combat := examples/characters/combat.c
HOST_TEST_EXTRA_test_skin_parallel := src/graphics/3d/animation/skin_parallel.cpp $(HOST_TEST_EXTRA_test_skin3d)

build/tests/test_character_asset: build/generated/character_examples/disc/high_blue.ska build/generated/character_examples/disc/high_red.ska
HOST_TEST_EXTRA_test_anim3d_logic := src/graphics/3d/animation/api.cpp src/graphics/3d/geometry/model.cpp src/graphics/3d/geometry/mesh.cpp src/core/geometry/mesh_api.cpp src/core/math3d/api.cpp
HOST_TEST_EXTRA_test_anim3d_async_lifetime := src/graphics/3d/animation/api.cpp src/graphics/3d/geometry/model.cpp src/graphics/3d/geometry/mesh.cpp src/core/geometry/mesh_api.cpp src/core/math3d/api.cpp
HOST_TEST_EXTRA_test_pacman_game := examples/pacman/pacman_game.c examples/pacman/pacman_level.c examples/pacman/pacman_stages.c src/physics/2d/grid.cpp src/physics/2d/collision.cpp src/core/math3d/api.cpp
HOST_TEST_EXTRA_test_skybridge_game := src/graphics/3d/rendering/surface.cpp
HOST_TEST_EXTRA_test_skybridge_fade := src/graphics/3d/rendering/surface.cpp
HOST_TEST_EXTRA_test_math2d := src/core/math2d/api.cpp
HOST_TEST_EXTRA_test_character2 := src/physics/2d/character2.cpp src/physics/2d/terrain2.cpp src/core/math2d/api.cpp
HOST_TEST_EXTRA_test_physics2_world := src/physics/2d/physics2_world.cpp src/physics/spatial/2d.cpp src/physics/2d/collision.cpp
HOST_TEST_EXTRA_test_path2 := src/physics/2d/path2.cpp src/core/math2d/api.cpp
HOST_TEST_EXTRA_test_follow_camera2d := src/physics/2d/follow_camera2d.cpp src/core/math2d/api.cpp
HOST_TEST_EXTRA_test_stage_map2 := src/graphics/vdp2/stage_map2.cpp
HOST_TEST_EXTRA_test_vdp2_tile_stream := src/graphics/vdp2/stage_map2.cpp
HOST_TEST_EXTRA_test_entity_stream2 := src/physics/spatial/entity_stream2.cpp src/physics/2d/follow_camera2d.cpp src/core/math2d/api.cpp
HOST_TEST_EXTRA_test_terrain2 := src/physics/2d/terrain2.cpp src/core/math2d/api.cpp src/physics/2d/grid.cpp src/physics/2d/collision.cpp
HOST_TEST_EXTRA_test_collide2d_logic := src/physics/2d/collision.cpp
HOST_TEST_EXTRA_test_spatial_logic := src/physics/spatial/2d.cpp src/physics/2d/collision.cpp
HOST_TEST_EXTRA_test_physics_logic := src/physics/2d/collision.cpp src/physics/2d/grid.cpp
HOST_TEST_EXTRA_test_physics3_world := src/physics/3d/world.cpp src/physics/3d/collision.cpp src/physics/3d/sweep.cpp src/physics/3d/sweep_full.cpp
HOST_TEST_EXTRA_test_collide3d_sweep := src/physics/3d/sweep.cpp
HOST_TEST_EXTRA_test_collide3d_sweep_full := src/physics/3d/sweep_full.cpp
HOST_TEST_EXTRA_test_collide3d_logic :=
HOST_TEST_EXTRA_test_spatial3_api := src/physics/spatial/3d.cpp src/physics/3d/collision.cpp

# Examples that ship a host test of their own say how to build it in examples/<name>/host_test.mk.
-include $(wildcard examples/*/host_test.mk)
-include $(HOST_TEST_BINS:=.d)

$(BUILD_DIR)/tests/%: tests/host/%.cpp
	@mkdir -p $(dir $@)
	$(HOST_CXX) $(HOST_CXXFLAGS) $(DEPFLAGS) $< $(HOST_TEST_EXTRA_$*) -o $@
	# A multi-source link overwrites the compiler's .d for each input. Merge
	# dependencies from every input so header edits rebuild the actual test.
	$(HOST_CXX) $(HOST_CXXFLAGS) -MM -MP -MT $@ $< $(HOST_TEST_EXTRA_$*) > $@.d

test: $(HOST_TEST_BINS)
	@fail=0; skipped=0; \
	for t in $(HOST_TEST_BINS); do \
		./$$t; result=$$?; \
		if [ $$result -eq 77 ]; then skipped=$$((skipped + 1)); \
		elif [ $$result -ne 0 ]; then fail=1; fi; \
	done; \
	if [ $$fail -ne 0 ]; then echo "[test] FAILED"; exit 1; fi; \
	if [ $$skipped -eq 0 ]; then echo "[test] all host tests passed"; \
	else echo "[test] supported host tests passed; $$skipped explicitly unavailable (exit 77)"; fi
	$(PYTHON) -m unittest tests.test_boot_image tests.test_native_ip tests.test_vdp2_support_matrix -v
	@for t in $(HOST_TOOL_TESTS); do \
		echo "[test] $$t"; \
		$(PYTHON) $$t || exit 1; \
	done

# -- Alvos utilitarios ------------------------------------------
.PHONY: print-build-paths
print-build-paths:
	@echo library=$(LIB_OBJ_ROOT)
	@echo objects=$(APP_OBJ_ROOT)
	@echo artifacts=$(OUTPUT_DIR)
	@echo profile=$(LIB_PROFILE_METRICS)
	@echo fault=$(LIB_FAULT_MODE)

list-examples:
	@echo "Available examples:"
	@for e in $(EXAMPLES); do echo "  $$e"; done

examples-all:
	@for e in $(filter-out $(EXAMPLES_ALL_SKIP),$(EXAMPLES)); do \
		echo "[build] $$e"; \
		$(MAKE) --no-print-directory EXAMPLE=$$e all || exit $$?; \
	done

clean:
	rm -rf $(BUILD_DIR)

endif # LIBSATURN_LOCK_TRAMPOLINE
