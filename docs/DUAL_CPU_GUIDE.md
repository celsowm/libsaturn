# Dual CPU Programming Guide

## Overview

The Saturn has two SH-2 processors that can run in parallel:
- **Master CPU**: Controls game logic, input, sound
- **Slave CPU**: Handles geometry transformation, VDP1 command generation

## Communication

### Shared Memory Protocol

Use the `SharedData` structure with uncached access:

```c
typedef struct {
    volatile uint32_t state; // 0=Master Writing, 1=Slave Working
    struct { int16_t x, y; } input;
    uint32_t _padding[4];    // Cache line padding
} __attribute__((aligned(16))) SharedData;
```

### Master to Slave Flow

```c
// Master CPU
shared.state = SHARED_STATE_SLAVE_WORKING;
shared.input.x = 100;
dualcpu_signal_slave();
dualcpu_wait_for_slave();

// Slave CPU
while (shared.state == SHARED_STATE_MASTER_WRITING);
// Process data...
shared.state = SHARED_STATE_MASTER_WRITING;
```

## Cache Coherency

### Critical Rule: Always use uncached pointers for sync flags

```c
volatile uint32_t* state = UNCACHED(&shared.state);
while (*state == SHARED_STATE_MASTER_WRITING);
```

### Cache Purging

Before modifying shared data structures:

```c
dualcpu_purge_cache(CPU_MASTER);
dualcpu_purge_cache(CPU_SLAVE);
```

## Memory Layout

### Slave Code (High WRAM)
- **Address**: 0x06000000
- **Speed**: 32-bit bus (faster)
- **Attribute**: `__attribute__((section(".slave_code")))`

### Master Code (Low WRAM)
- **Address**: 0x06004000
- **Speed**: 16-bit bus (slower)
- **Default** section

## Work Distribution

### Master CPU Responsibilities
- Input handling
- Game logic
- Collision detection (gameplay)
- Sound management
- VDP2 background updates

### Slave CPU Responsibilities
- 3D matrix transformations
- Vertex projection
- VDP1 command list generation
- Texture coordinate calculation

## Performance Tips

1. **Minimize master/slave handshakes** - Each sync costs cycles
2. **Batch operations** - Process multiple objects per frame
3. **Use DMA for large transfers** - Free CPU cycles
4. **Prefer uncached reads for sync flags** - Avoid cache pollution

## Common Pitfalls

### ❌ Don't use cached pointers for sync
```c
while (shared.state == MASTER_WRITING); // WRONG - cached read
```

### ✅ Always use UNCACHED macro
```c
volatile uint32_t* state = UNCACHED(&shared.state);
while (*state == MASTER_WRITING); // CORRECT
```

### ❌ Don't modify shared structures without syncing
```c
shared.input.x = 100; // Might be overwritten by slave
```

### ✅ Always use state machine
```c
while (*state != MASTER_WRITING); // Wait for slave
shared.input.x = 100; // Now safe to modify
*state = SLAVE_WORKING; // Signal slave
```
