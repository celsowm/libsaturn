TARGET      := sh2eb-elf
CC          := $(TARGET)-gcc
CXX         := $(TARGET)-g++
AR          := $(TARGET)-ar
OBJCOPY     := $(TARGET)-objcopy
PYTHON      ?= python

MKISOFS     := $(shell command -v mkisofs 2>/dev/null || command -v genisoimage 2>/dev/null || command -v xorrisofs 2>/dev/null)

IP_PROFILE  ?= current
IP_TEMPLATE_KIND ?= yaul

VALID_IP_PROFILES  := current safe
VALID_IP_TEMPLATE_KINDS := yaul sbl minimal yaul_fixed region_free minimal_boot correct final

ifneq ($(filter $(IP_PROFILE),$(VALID_IP_PROFILES)),$(IP_PROFILE))
$(error IP_PROFILE invalido '$(IP_PROFILE)'. Use um entre: $(VALID_IP_PROFILES))
endif

ifneq ($(filter $(IP_TEMPLATE_KIND),$(VALID_IP_TEMPLATE_KINDS)),$(IP_TEMPLATE_KIND))
$(error IP_TEMPLATE_KIND invalido '$(IP_TEMPLATE_KIND)'. Use um entre: $(VALID_IP_TEMPLATE_KINDS))
endif

BUILD_DIR   := build
ISO_ROOT    := iso_root
IP_BIN      := ip.bin
ifeq ($(IP_TEMPLATE_KIND),yaul)
IP_TEMPLATE := assets/boot/ip_yaul_template.bin
else ifeq ($(IP_TEMPLATE_KIND),yaul_fixed)
IP_TEMPLATE := assets/boot/ip_yaul_fixed_template.bin
else ifeq ($(IP_TEMPLATE_KIND),region_free)
IP_TEMPLATE := assets/boot/ip_region_free_template.bin
else ifeq ($(IP_TEMPLATE_KIND),correct)
IP_TEMPLATE := assets/boot/ip_correct_template.bin
else ifeq ($(IP_TEMPLATE_KIND),final)
IP_TEMPLATE := assets/boot/ip_final_template.bin
else ifeq ($(IP_TEMPLATE_KIND),minimal_boot)
IP_TEMPLATE := assets/boot/ip_minimal_boot.bin
else ifeq ($(IP_TEMPLATE_KIND),minimal)
IP_TEMPLATE := assets/boot/ip_minimal_template.bin
else
IP_TEMPLATE := assets/boot/ip_sbl_template.bin
endif
VARIANT_NAME := mvp-$(IP_TEMPLATE_KIND)-$(IP_PROFILE)
VARIANT_ISO  := $(BUILD_DIR)/$(VARIANT_NAME).iso
VARIANT_CUE  := $(BUILD_DIR)/$(VARIANT_NAME).cue
APP_LOAD_ADDR_HEX := 06004000
MAX_APP_BIN_BYTES := 983040

BASE_CFLAGS := -m2 -mb -O2 -ffreestanding -fomit-frame-pointer -Wall -Wextra -Iinclude -I.
CFLAGS      := $(BASE_CFLAGS)
CXXFLAGS    := $(CFLAGS) -std=c++20 -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit
ASFLAGS     := -m2 -mb
LDFLAGS     := -m2 -mb -nostdlib -Wl,-T,src/core/saturn.ld -Wl,-Map,$(BUILD_DIR)/mvp.map -Wl,--gc-sections

LIB_CPP_SRCS := \
	src/core/runtime_state.cpp \
	src/core/core_api.cpp \
	src/core/video_api.cpp \
	src/core/input_api.cpp \
	src/core/vdp1_api.cpp \
	src/core/vdp2_api.cpp \
	src/hal/vdp1.cpp \
	src/hal/vdp2.cpp \
	src/hal/scu.cpp \
	src/hal/smpc.cpp

LIB_C_SRCS := src/core/newlib_stubs.c \
	src/core/early_init.c
CRT_SRCS   := src/core/crt0.s

EXAMPLE ?= mvp_2d_scene

APP_C_SRCS := $(wildcard examples/$(EXAMPLE)/*.c) $(wildcard examples/common/*.c)
APP_OBJS   := $(patsubst %.c,$(BUILD_DIR)/%.o,$(APP_C_SRCS))

ELF := $(BUILD_DIR)/$(EXAMPLE).elf
BIN := $(BUILD_DIR)/$(EXAMPLE).bin

HOST_CXX ?= g++
HOST_BUILD_DIR := build-host

EXAMPLES := $(notdir $(wildcard examples/*))

LIB_CPP_OBJS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(LIB_CPP_SRCS))
LIB_C_OBJS   := $(patsubst %.c,$(BUILD_DIR)/%.o,$(LIB_C_SRCS))
APP_OBJS     := $(patsubst %.c,$(BUILD_DIR)/%.o,$(APP_C_SRCS))
CRT_OBJS     := $(patsubst %.s,$(BUILD_DIR)/%.o,$(CRT_SRCS))

LIB_OBJS     := $(LIB_CPP_OBJS) $(LIB_C_OBJS)
ALL_OBJS     := $(LIB_OBJS) $(APP_OBJS) $(CRT_OBJS)

LIBRARY      := $(BUILD_DIR)/libsaturn.a
ELF          := $(BUILD_DIR)/mvp.elf
BIN          := $(BUILD_DIR)/mvp.bin
ISO          := $(BUILD_DIR)/mvp.iso
CUE          := $(BUILD_DIR)/mvp.cue

.PHONY: all clean dirs check-tools test test-host examples-all assets

all: check-tools dirs $(ISO) $(LIBRARY)

check-tools:
	@if ! command -v $(CC) >/dev/null 2>&1; then echo "Erro: $(CC) nao encontrado no PATH"; exit 1; fi
	@if [ -z "$(MKISOFS)" ]; then echo "Erro: mkisofs/genisoimage/xorrisofs nao encontrado"; exit 1; fi
	@echo "[profiles] IP_PROFILE=$(IP_PROFILE)"
	@echo "[profiles] IP_TEMPLATE_KIND=$(IP_TEMPLATE_KIND)"

dirs:
	@mkdir -p $(BUILD_DIR) $(BUILD_DIR)/src/core $(BUILD_DIR)/src/hal $(BUILD_DIR)/examples/mvp_2d_scene $(ISO_ROOT)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.s
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

$(LIBRARY): $(LIB_OBJS)
	$(AR) rcs $@ $^

$(ELF): $(CRT_OBJS) $(APP_OBJS) $(LIBRARY)
	$(CXX) $(LDFLAGS) -o $@ $(CRT_OBJS) $(APP_OBJS) -L$(BUILD_DIR) -lsaturn -lgcc

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@
	$(PYTHON) -c "import sys; from pathlib import Path; p = Path('$(BIN)'); size = p.stat().st_size; max_size = $(MAX_APP_BIN_BYTES); print(f'[check] {p} size={size} bytes (max {max_size})'); sys.exit(0 if size <= max_size else 1)"

$(IP_BIN): $(BIN) $(IP_TEMPLATE)
	$(PYTHON) -c "import struct,sys; from pathlib import Path; app_size=Path('$(BIN)').stat().st_size; ip_profile='$(IP_PROFILE)'; first_size=0 if ip_profile=='safe' else app_size; tmpl=bytearray(Path('$(IP_TEMPLATE)').read_bytes()); sys.exit(1) if len(tmpl) > 0x8000 else None; tmpl.extend(b'\x00' * (0x8000 - len(tmpl))); src=bytearray(tmpl); struct.pack_into('>I', src, 0x0F0, 0x$(APP_LOAD_ADDR_HEX)); struct.pack_into('>I', src, 0x0F4, first_size); Path('$(IP_BIN)').write_bytes(src); print(f'[gen] ip.bin profile={ip_profile} size={len(src)} first_read=0x{0x$(APP_LOAD_ADDR_HEX):08X} first_size=0x{first_size:08X}')"
	$(PYTHON) -c "import struct,sys; from pathlib import Path; app_size=Path('$(BIN)').stat().st_size; ip_profile='$(IP_PROFILE)'; expected_first_size=0 if ip_profile=='safe' else app_size; tmpl=bytearray(Path('$(IP_TEMPLATE)').read_bytes()); sys.exit(1) if len(tmpl) > 0x8000 else None; tmpl.extend(b'\x00' * (0x8000 - len(tmpl))); d=Path('$(IP_BIN)').read_bytes(); magic=d[0:16]; area_symbols=d[0x40:0x4A]; first_read=struct.unpack('>I', d[0x0F0:0x0F4])[0]; first_size=struct.unpack('>I', d[0x0F4:0x0F8])[0]; security_ok=(d[0x0100:0x0600]==bytes(tmpl[0x0100:0x0600])); area_obj_ok=(d[0x0E00:0x8000]==bytes(tmpl[0x0E00:0x8000])); header_unchanged=(d[0x0010:0x00F0]==bytes(tmpl[0x0010:0x00F0])); errs=[]; errs += ['size'] if len(d)!=0x8000 else []; errs += ['magic'] if magic!=b'SEGA SEGASATURN ' else []; errs += ['header_modified'] if not header_unchanged else []; errs += ['first_read'] if first_read!=0x$(APP_LOAD_ADDR_HEX) else []; errs += ['first_size'] if first_size!=expected_first_size else []; errs += ['security_block_modified'] if not security_ok else []; errs += ['area_code_object_modified'] if not area_obj_ok else []; print(f'[check] ip.bin profile={ip_profile} len={len(d)} magic={magic!r} area={area_symbols!r} first_read=0x{first_read:08X} first_size=0x{first_size:08X} header_unchanged={header_unchanged} security_ok={security_ok} area_obj_ok={area_obj_ok}'); print('[check] ip.bin FAIL: ' + ', '.join(errs)) if errs else None; sys.exit(1 if errs else 0)"

$(ISO): $(BIN) $(IP_BIN)
	@mkdir -p $(ISO_ROOT)
	rm -f $(ISO_ROOT)/0.BIN $(ISO_ROOT)/1ST_READ.BIN
	cp $(BIN) $(ISO_ROOT)/0.BIN
	cp $(BIN) $(ISO_ROOT)/1ST_READ.BIN
	$(MKISOFS) \
		-sysid "SEGA SATURN" \
		-volid "LIBSATURN" \
		-publisher "LIBSATURN" \
		-iso-level 1 \
		-l \
		-G $(IP_BIN) \
		-o $@ \
		$(ISO_ROOT)
	$(PYTHON) -c "import struct,sys; from pathlib import Path; app_size=Path('$(BIN)').stat().st_size; ip_profile='$(IP_PROFILE)'; expected_first_size=0 if ip_profile=='safe' else app_size; iso=Path('$(ISO)').read_bytes(); h=iso[:2048]; magic=h[0:16]; first_read=struct.unpack('>I', h[0x0F0:0x0F4])[0]; first_size=struct.unpack('>I', h[0x0F4:0x0F8])[0]; ok=(magic==b'SEGA SEGASATURN ' and first_read==0x$(APP_LOAD_ADDR_HEX) and first_size==expected_first_size); print(f'[check] iso profile={ip_profile} lba0 magic={magic!r} first_read=0x{first_read:08X} first_size=0x{first_size:08X}'); sys.exit(0 if ok else 1)"
	@echo 'FILE "mvp.iso" BINARY' > $(CUE)
	@echo '  TRACK 01 MODE1/2048' >> $(CUE)
	@echo '    INDEX 01 00:00:00' >> $(CUE)
	cp $(ISO) $(VARIANT_ISO)
	@echo "FILE \"$(VARIANT_NAME).iso\" BINARY" > $(VARIANT_CUE)
	@echo '  TRACK 01 MODE1/2048' >> $(VARIANT_CUE)
	@echo '    INDEX 01 00:00:00' >> $(VARIANT_CUE)
	@echo "[variant] $(VARIANT_ISO)"
	@echo "[variant] $(VARIANT_CUE)"

assets:
	$(PYTHON) tools/convert_indexed8.py --input assets/demo.raw --width 16 --height 16 --palette assets/demo.pal.txt --out-prefix build/demo

test-host:
	@mkdir -p $(HOST_BUILD_DIR)
	$(HOST_CXX) -std=c++20 -Wall -Wextra -Iinclude -I. \
		tests/host/test_core_logic.cpp \
		tests/host/test_input_logic.cpp \
		tests/host/test_vdp1_logic.cpp \
		tests/host/test_vdp2_logic.cpp \
		-o $(HOST_BUILD_DIR)/host_tests
	$(HOST_BUILD_DIR)/host_tests

test: test-host
	$(PYTHON) -m unittest tests/test_asset_converter.py tests/test_gen_ip_bin.py

examples-all:
	@for e in $(EXAMPLES); do $(MAKE) EXAMPLE=$$e all; done

clean:
	rm -rf $(BUILD_DIR) $(ISO_ROOT) $(IP_BIN) || true
