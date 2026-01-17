# Memory Map

## Saturn Memory Layout

### High WRAM (0x06000000 - 0x0603FFFF)
- **Size**: 256KB
- **Speed**: 32-bit bus (faster)
- **Usage**: Slave CPU code and stack
- **Section**: `.slave_text` in linker script

### Low WRAM (0x06004000 - 0x060FFFFF)
- **Size**: 1008KB
- **Speed**: 16-bit bus (slower)
- **Usage**: Master CPU code, data, stack
- **Section**: `.text`, `.data`, `.bss` in linker script

### VDP1 VRAM (0x25C00000 - 0x25C7FFFF)
- **Size**: 512KB
- **Usage**: VDP1 command lists, sprite patterns, textures
- **Access**: 16-bit writes recommended

### VDP2 VRAM (0x25E00000 - 0x25EFFFFF)
- **Size**: 512KB
- **Usage**: Background patterns, maps, color tables
- **Access**: 16-bit writes recommended

### SCU RAM (0x25F00000 - 0x25F000FF)
- **Size**: 256 bytes
- **Usage**: DMA parameters, SCU registers

## Uncached Memory Access

To bypass CPU cache, OR the address with `0x20000000`:

```c
#define UNCACHED(ptr) ((void*)((uint32_t)(ptr) | 0x20000000))

volatile uint32_t* sync = UNCACHED(&shared.state);
```

**Critical for**: Master/Slave communication flags

## Stack Allocation

- **Master Stack**: `0x06004000 + 0x000FC000 = 0x06100000`
- **Slave Stack**: `0x06000000 + 0x00040000 = 0x06040000`

## Memory Management Guidelines

1. **Use High WRAM for slave code** - Faster access
2. **Place shared data in Low WRAM** - Both CPUs can access
3. **Use uncached pointers for sync** - Prevents stale reads
4. **Align structures to 16 bytes** - Cache line optimization
