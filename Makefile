# ================================================================
# libsaturn Makefile - Build system para Sega Saturn
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

# Python: prefere o do Windows (tem Pillow), fallback para MSYS2
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
$(error IP_PROFILE invalido '$(IP_PROFILE)'. Use: $(VALID_IP_PROFILES))
endif
ifneq ($(filter $(IP_TEMPLATE_KIND),$(VALID_IP_TEMPLATE_KINDS)),$(IP_TEMPLATE_KIND))
$(error IP_TEMPLATE_KIND invalido '$(IP_TEMPLATE_KIND)'. Use: $(VALID_IP_TEMPLATE_KINDS))
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

# -- Exemplo (descoberta automatica) ----------------------------
# Cada exemplo pode ter um Makefile.inc definindo:
#   EXAMPLE_ASSETS  = lista de assets (ex: $(GENERATED_DIR)/sega_bg/bg.c)
#   EXAMPLE_HEADERS = headers gerados
#   EXAMPLE_DEPS    = dependencias para geracao
#   EXAMPLE_RESIZE  = largura altura (ex: 320 224)
#   EXAMPLE_INPUT   = caminho da imagem original
# ---------------------------------------------------------------
EXAMPLE_DIR     := examples/$(EXAMPLE)
EXAMPLE_SRCS    := $(wildcard $(EXAMPLE_DIR)/*.c) $(wildcard examples/common/*.c)
EXAMPLE_OBJS    := $(patsubst %.c,$(BUILD_DIR)/%.o,$(EXAMPLE_SRCS))
EXAMPLE_HEADERS :=

# Incluir configuracao do exemplo se existir
EXAMPLE_INC := $(EXAMPLE_DIR)/Makefile.inc
ifneq ($(wildcard $(EXAMPLE_INC)),)
  include $(EXAMPLE_INC)
endif

ALL_APP_OBJS := $(EXAMPLE_OBJS) $(EXAMPLE_ASSETS:.c=.o)
ALL_HEADERS  := $(EXAMPLE_HEADERS)

# -- Artefatos --------------------------------------------------
ELF := $(BUILD_DIR)/$(EXAMPLE).elf
BIN := $(BUILD_DIR)/$(EXAMPLE).bin
ISO := $(BUILD_DIR)/$(EXAMPLE).iso
CUE := $(BUILD_DIR)/$(EXAMPLE).cue

# -- Exemplos disponiveis ---------------------------------------
EXAMPLES := $(filter-out common,$(notdir $(wildcard examples/*)))

.PHONY: all clean dirs check-tools examples-all list-examples bake

all: check-tools dirs $(ELF) $(ISO) $(LIBRARY)

# -- Verificacoes -----------------------------------------------
check-tools:
	@if ! command -v $(CC) >/dev/null 2>&1; then \
		echo "Erro: $(CC) nao encontrado no PATH"; exit 1; fi
	@if [ -z "$(MKISOFS)" ]; then \
		echo "Erro: mkisofs/genisoimage/xorrisofs nao encontrado"; exit 1; fi
	@echo "[profiles] EXAMPLE=$(EXAMPLE) IP_PROFILE=$(IP_PROFILE) IP_TEMPLATE=$(IP_TEMPLATE_KIND)"

dirs:
	@mkdir -p $(BUILD_DIR) $(GENERATED_DIR) $(ISO_ROOT)
	@mkdir -p $(dir $(LIB_CPP_OBJS)) $(dir $(LIB_C_OBJS)) $(dir $(CRT_OBJS))
	@mkdir -p $(dir $(EXAMPLE_OBJS)) $(dir $(ALL_APP_OBJS))

# -- Geracao de assets ------------------------------------------
# Se assets pré-convertidos existem em assets/prebuilt/, usá-los diretamente.
# Senão, gerar via convert_indexed8.py (requer Pillow).
PREBUILT_DIR := assets/prebuilt

# Verificar se existe asset pré-convertido para este exemplo
PREBUILT_ASSET_H := $(wildcard $(PREBUILT_DIR)/$(EXAMPLE)/*.h)
ifneq ($(PREBUILT_ASSET_H),)
  # Usar asset pré-convertido
  PREBUILT_ASSET_C := $(PREBUILT_ASSET_H:.h=.c)
  EXAMPLE_ASSETS := $(PREBUILT_ASSET_C)
  EXAMPLE_HEADERS := $(PREBUILT_ASSET_H)
  EXAMPLE_PREBUILT := yes
  $(info [assets] Usando prebuilt: $(PREBUILT_ASSET_H))
else
  # Gerar asset via convert_indexed8.py (requer Pillow)
  ifneq ($(EXAMPLE_INPUT),)
  ifneq ($(EXAMPLE_RESIZE),)
  $(EXAMPLE_ASSETS): $(EXAMPLE_INPUT) $(TOOLS)/convert_indexed8.py
	@mkdir -p $(dir $@)
	$(PYTHON) $(TOOLS)/convert_indexed8.py \
		--input $(EXAMPLE_INPUT) \
		--resize $(EXAMPLE_RESIZE) \
		--out-prefix $(EXAMPLE_ASSET_PREFIX)
  endif
  endif
endif

# -- Compilacao -------------------------------------------------
# Regra para arquivos C (exemplos, biblioteca, assets gerados)
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Regra para assets pré-convertidos (assets/prebuilt/*.c)
$(BUILD_DIR)/assets/prebuilt/%.o: assets/prebuilt/%.c
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
	@echo "Exemplos disponiveis:"
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
