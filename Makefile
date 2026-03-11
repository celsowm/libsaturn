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
IP_STUB_SRC := src/core/ip_stub.s
IP_STUB_OBJ := $(BUILD_DIR)/ip_stub.o
IP_STUB_BIN := $(BUILD_DIR)/ip_stub.bin
APP_LOAD_ADDR_HEX := 06010000
IP_LOAD_ADDR_HEX  := 06004000
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

$(IP_STUB_BIN): $(IP_STUB_SRC)
	@mkdir -p $(dir $(IP_STUB_OBJ))
	$(CC) $(ASFLAGS) -c $(IP_STUB_SRC) -o $(IP_STUB_OBJ)
	$(OBJCOPY) -O binary $(IP_STUB_OBJ) $(IP_STUB_BIN)

$(IP_BIN): $(IP_STUB_BIN) $(BIN)
	$(PYTHON) tools/gen_ip_bin.py --output $(IP_BIN) --stub $(IP_STUB_BIN) --ip-load-address $(IP_LOAD_ADDR_HEX) --first-read-address $(APP_LOAD_ADDR_HEX) --first-read-file $(BIN)
	$(PYTHON) -c "import struct,sys; from pathlib import Path; d=Path('$(IP_BIN)').read_bytes(); magic=d[0:16]; area=d[0x03C:0x046]; first_read=struct.unpack('>I', d[0x0E0:0x0E4])[0]; ok=(magic==b'SEGA SEGASATURN ' and area==b'JT  UB E  ' and first_read==0x$(APP_LOAD_ADDR_HEX)); print(f'[check] ip.bin magic={magic!r} area={area!r} first_read=0x{first_read:08X}'); sys.exit(0 if ok else 1)"

$(ISO): $(BIN) $(IP_BIN)
	@mkdir -p $(ISO_ROOT)
	cp $(BIN) $(ISO_ROOT)/0.BIN
	$(MKISOFS) \
		-sysid "SEGA SATURN" \
		-volid "LIBSATURN" \
		-publisher "LIBSATURN" \
		-iso-level 1 \
		-l \
		-G $(IP_BIN) \
		-o $@ \
		$(ISO_ROOT)
	$(PYTHON) -c "import struct,sys; from pathlib import Path; iso=Path('$(ISO)').read_bytes(); h=iso[:2048]; magic=h[0:16]; area=h[0x03C:0x046]; first_read=struct.unpack('>I', h[0x0E0:0x0E4])[0]; ok=(magic==b'SEGA SEGASATURN ' and area==b'JT  UB E  ' and first_read==0x$(APP_LOAD_ADDR_HEX)); print(f'[check] iso lba0 magic={magic!r} area={area!r} first_read=0x{first_read:08X}'); sys.exit(0 if ok else 1)"
	@echo 'FILE "mvp.iso" BINARY' > $(CUE)
	@echo '  TRACK 01 MODE1/2048' >> $(CUE)
	@echo '    INDEX 01 00:00:00' >> $(CUE)

assets:
	$(PYTHON) tools/convert_indexed8.py --input assets/demo.raw --width 16 --height 16 --palette assets/demo.pal.txt --out-prefix build/demo

test:
	$(PYTHON) -m unittest tests/test_asset_converter.py tests/test_gen_ip_bin.py

clean:
	rm -rf $(BUILD_DIR) $(ISO_ROOT)
