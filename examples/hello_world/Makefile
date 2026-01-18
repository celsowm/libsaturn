# Hello World Example Makefile for libsaturn
#
# This Makefile builds the hello_world example using libsaturn.

# ============================================
# CONFIGURATION
# ============================================

# Toolchain path (auto-detect if not set)
ifndef SATURN_TOOLCHAIN
    SATURN_TOOLCHAIN := $(shell \
        if exist "..\..\toolchains\sh-elf-gcc\bin\sh-elf-gcc.exe" echo ..\..\toolchains\sh-elf-gcc\bin && \
        if exist "..\..\..\toolchains\sh-elf-gcc\bin\sh-elf-gcc.exe" echo ..\..\..\toolchains\sh-elf-gcc\bin && \
        if exist "$(USERPROFILE)\saturn-sdk\sh-elf-gcc\bin\sh-elf-gcc.exe" echo $(USERPROFILE)\saturn-sdk\sh-elf-gcc\bin)
endif

# Paths
LIBSATURN_DIR := ..
TOOLCHAIN_BIN := $(SATURN_TOOLCHAIN)
LIB_DIR := $(LIBSATURN_DIR)/lib
INCLUDE_DIR := $(LIBSATURN_DIR)/include

# Executables
CC := $(TOOLCHAIN_BIN)/sh-elf-gcc.exe
AR := $(TOOLCHAIN_BIN)/sh-elf-ar.exe
LD := $(TOOLCHAIN_BIN)/sh-elf-ld.exe
OBJCOPY := $(TOOLCHAIN_BIN)/sh-elf-objcopy.exe

# Compiler flags
CFLAGS := -m2 -mb -O2 -fomit-frame-pointer
CFLAGS += -nostartfiles
CFLAGS += -I$(INCLUDE_DIR)
CFLAGS += -Wall

# Linker flags
LDFLAGS := -T$(LIBSATURN_DIR)/scripts/linker.ld
LDFLAGS += -Map=hello_world.map

# ============================================
# BUILD TARGETS
# ============================================

.PHONY: all clean run

all: hello_world.bin

hello_world.bin: main.c $(LIB_DIR)/libsaturn.a
	$(CC) $(CFLAGS) $(LDFLAGS) -o hello_world.elf main.c -L$(LIB_DIR) -lsaturn -lgcc
	$(OBJCOPY) -O binary hello_world.elf hello_world.bin

$(LIB_DIR)/libsaturn.a:
	@echo "Building libsaturn..."
	cd $(LIBSATURN_DIR) && $(MAKE) -f build.bat

clean:
	del /Q *.o *.elf *.bin *.map 2>nul

run: hello_world.bin
	@echo "Hello World binary created: hello_world.bin"
	@echo "Run with emulator: emu.exe hello_world.bin"

# ============================================
# HELP
# ============================================

help:
	@echo "Hello World Example Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  make        - Build hello_world.bin"
	@echo "  make clean  - Remove build artifacts"
	@echo "  make run    - Build and show binary location"
	@echo "  make help   - Show this help"
	@echo ""
	@echo "Output:"
	@echo "  hello_world.bin - Final binary for emulator/hardware"
	@echo "  hello_world.map - Linker map file"
