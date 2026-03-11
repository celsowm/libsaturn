TARGET      := sh2eb-elf
CC          := $(TARGET)-gcc
CXX         := $(TARGET)-g++
AR          := $(TARGET)-ar
OBJCOPY     := $(TARGET)-objcopy
PYTHON      ?= python

MKISOFS     := $(shell command -v mkisofs 2>/dev/null || command -v genisoimage 2>/dev/null || command -v xorrisofs 2>/dev/null)

BUILD_DIR   := build
ISO_ROOT    := iso_root
IP_BIN      := ip.bin
IP_TEMPLATE := assets/boot/ip_sbl_template.bin
APP_LOAD_ADDR_HEX := 06004000
MAX_APP_BIN_BYTES := 983040

CFLAGS      := -m2 -mb -O2 -ffreestanding -fomit-frame-pointer -Wall -Wextra -Iinclude -I.
CXXFLAGS    := $(CFLAGS) -std=c++20 -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit
ASFLAGS     := -m2 -mb
LDFLAGS     := -m2 -mb -nostdlib -Wl,-T,src/core/saturn.ld -Wl,-Map,$(BUILD_DIR)/mvp.map -Wl,--gc-sections

LIB_CPP_SRCS := \
	src/core/saturn.cpp \
	src/hal/vdp1.cpp \
	src/hal/vdp2.cpp \
	src/hal/scu.cpp \
	src/hal/smpc.cpp

LIB_C_SRCS := src/core/newlib_stubs.c
APP_C_SRCS := examples/mvp_2d_scene/main.c
CRT_SRCS   := src/core/crt0.s

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

.PHONY: all clean dirs check-tools test assets

all: check-tools dirs $(ISO) $(LIBRARY)

check-tools:
	@if ! command -v $(CC) >/dev/null 2>&1; then echo "Erro: $(CC) nao encontrado no PATH"; exit 1; fi
	@if [ -z "$(MKISOFS)" ]; then echo "Erro: mkisofs/genisoimage/xorrisofs nao encontrado"; exit 1; fi

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
	$(PYTHON) -c "import struct,sys; from pathlib import Path; app_size=Path('$(BIN)').stat().st_size; src=bytearray(Path('$(IP_TEMPLATE)').read_bytes()); sys.exit(1) if len(src) > 0x8000 else None; src.extend(b'\x00' * (0x8000 - len(src))); wf=lambda o,s,t:(src.__setitem__(slice(o,o+s), b' '*s), src.__setitem__(slice(o,o+len(t.encode('ascii')[:s])), t.encode('ascii')[:s])); wf(0x010,16,'LIBSATURN'); wf(0x020,10,'T-00000G  '); wf(0x02A,6,'V1.000'); wf(0x030,8,'20260311'); wf(0x060,112,'LIBSATURN MVP'); struct.pack_into('>I', src, 0x0F0, 0x$(APP_LOAD_ADDR_HEX)); struct.pack_into('>I', src, 0x0F4, app_size); Path('$(IP_BIN)').write_bytes(src); print(f'[gen] ip.bin from template size={len(src)} first_read=0x{0x$(APP_LOAD_ADDR_HEX):08X} first_size=0x{app_size:08X}')"
	$(PYTHON) -c "import struct,sys; from pathlib import Path; app_size=Path('$(BIN)').stat().st_size; d=Path('$(IP_BIN)').read_bytes(); magic=d[0:16]; first_read=struct.unpack('>I', d[0x0F0:0x0F4])[0]; first_size=struct.unpack('>I', d[0x0F4:0x0F8])[0]; ok=(len(d)==0x8000 and magic==b'SEGA SEGASATURN ' and first_read==0x$(APP_LOAD_ADDR_HEX) and first_size==app_size); print(f'[check] ip.bin len={len(d)} magic={magic!r} first_read=0x{first_read:08X} first_size=0x{first_size:08X}'); sys.exit(0 if ok else 1)"

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
	$(PYTHON) -c "import struct,sys; from pathlib import Path; app_size=Path('$(BIN)').stat().st_size; iso=Path('$(ISO)').read_bytes(); h=iso[:2048]; magic=h[0:16]; first_read=struct.unpack('>I', h[0x0F0:0x0F4])[0]; first_size=struct.unpack('>I', h[0x0F4:0x0F8])[0]; ok=(magic==b'SEGA SEGASATURN ' and first_read==0x$(APP_LOAD_ADDR_HEX) and first_size==app_size); print(f'[check] iso lba0 magic={magic!r} first_read=0x{first_read:08X} first_size=0x{first_size:08X}'); sys.exit(0 if ok else 1)"
	@echo 'FILE "mvp.iso" BINARY' > $(CUE)
	@echo '  TRACK 01 MODE1/2048' >> $(CUE)
	@echo '    INDEX 01 00:00:00' >> $(CUE)

assets:
	$(PYTHON) tools/convert_indexed8.py --input assets/demo.raw --width 16 --height 16 --palette assets/demo.pal.txt --out-prefix build/demo

test:
	$(PYTHON) -m unittest tests/test_asset_converter.py tests/test_gen_ip_bin.py

clean:
	rm -rf $(BUILD_DIR) $(ISO_ROOT) || true
