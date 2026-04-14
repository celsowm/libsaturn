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
#   EXAMPLE_DEPS    = dependencies for generation
#   EXAMPLE_RESIZE  = width height (e.g. 320 224)
#   EXAMPLE_INPUT   = original image path
# ---------------------------------------------------------------
EXAMPLE_DIR     := examples/$(EXAMPLE)
EXAMPLE_SRCS    := $(wildcard $(EXAMPLE_DIR)/*.c) $(wildcard examples/common/*.c)
EXAMPLE_OBJS    := $(patsubst %.c,$(BUILD_DIR)/%.o,$(EXAMPLE_SRCS))
EXAMPLE_HEADERS :=

# Include example config if it exists
EXAMPLE_INC := $(EXAMPLE_DIR)/Makefile.inc
ifneq ($(wildcard $(EXAMPLE_INC)),)
  include $(EXAMPLE_INC)
endif

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

.PHONY: all clean dirs check-tools examples-all list-examples bake

all: check-tools dirs $(ELF) $(ISO) $(LIBRARY)

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
    EXAMPLE_ASSETS := $(PREBUILT_C)
    EXAMPLE_HEADERS := $(PREBUILT_H)
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

# -- Compilation -------------------------------------------------
# Rule for C files (examples, library, generated assets)
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Rule for pre-converted assets (examples/*/prebuilt/*.c)
$(BUILD_DIR)/examples/%/prebuilt/%.o: examples/%/prebuilt/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.s
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

# -- Linkagem ---------------------------------------------------
$(LIBRARY): $(LIB_CPP_OBJS) $(LIB_C_OBJS)
	$(AR) rcs $@ $^

$(ELF): $(CRT_OBJS) $(ALL_APP_OBJS) $(LIBRARY)
	$(CXX) $(LDFLAGS) -o $@ $(CRT_OBJS) $(ALL_APP_OBJS) -L$(BUILD_DIR) -lsaturn -lgcc

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@
	$(PYTHON) $(TOOLS)/check_bin_size.py --bin $(BIN) --max-size $(MAX_APP_BIN_BYTES)

# -- IP.BIN -----------------------------------------------------
$(ISO_ROOT)/ip.bin: $(BIN) $(IP_TEMPLATE)
	@mkdir -p $(ISO_ROOT)
	@APP_SIZE=$$(wc -c < $(BIN)); \
	$(PYTHON) $(TOOLS)/gen_ip_bin.py \
		--template $(IP_TEMPLATE) \
		--output $(ISO_ROOT)/ip.bin \
		--app-size $$APP_SIZE \
		--profile $(IP_PROFILE) \
		--load-addr 0x$(APP_LOAD_ADDR_HEX); \
	$(PYTHON) $(TOOLS)/check_ip_bin.py \
		--ip-bin $(ISO_ROOT)/ip.bin \
		--template $(IP_TEMPLATE) \
		--expected-size $$APP_SIZE \
		--profile $(IP_PROFILE) \
		--load-addr 0x$(APP_LOAD_ADDR_HEX)

# -- ISO --------------------------------------------------------
$(ISO): $(BIN)
	@mkdir -p $(ISO_ROOT)
	@APP_SIZE=$$(wc -c < $(BIN)); \
	$(PYTHON) $(TOOLS)/gen_ip_bin.py \
		--template $(IP_TEMPLATE) \
		--output $(ISO_ROOT)/ip.bin \
		--app-size $$APP_SIZE \
		--profile $(IP_PROFILE) \
		--load-addr 0x$(APP_LOAD_ADDR_HEX) && \
	$(PYTHON) $(TOOLS)/check_ip_bin.py \
		--ip-bin $(ISO_ROOT)/ip.bin \
		--template $(IP_TEMPLATE) \
		--expected-size $$APP_SIZE \
		--profile $(IP_PROFILE) \
		--load-addr 0x$(APP_LOAD_ADDR_HEX) && \
	cp $(BIN) $(ISO_ROOT)/1ST_READ.BIN && \
	$(MKISOFS) \
		-sysid "SEGA SATURN" \
		-volid "$(EXAMPLE)" \
		-publisher "LIBSATURN" \
		-iso-level 1 \
		-l \
		-G $(ISO_ROOT)/ip.bin \
		-o $@ \
		$(ISO_ROOT) && \
	$(PYTHON) $(TOOLS)/check_iso.py \
		--iso $(ISO) \
		--expected-size $$APP_SIZE \
		--profile $(IP_PROFILE) \
		--load-addr 0x$(APP_LOAD_ADDR_HEX) && \
	$(PYTHON) $(TOOLS)/gen_cue.py \
		--iso-name $(EXAMPLE).iso \
		--cue-output $(CUE)

# -- CUE --------------------------------------------------------
$(CUE): $(ISO)
	$(PYTHON) $(TOOLS)/gen_cue.py \
		--iso-name $(EXAMPLE).iso \
		--cue-output $(CUE)

# -- Alvos utilitarios ------------------------------------------
list-examples:
	@echo "Available examples:"
	@for e in $(EXAMPLES); do echo "  $$e"; done

examples-all:
	@for e in $(EXAMPLES); do \
		echo "=== Building $$e ==="; \
		$(MAKE) EXAMPLE=$$e all || exit 1; \
	done

clean:
	rm -rf $(BUILD_DIR) $(ISO_ROOT) || true

bake:
	$(PYTHON) $(TOOLS)/bake-assets.py
