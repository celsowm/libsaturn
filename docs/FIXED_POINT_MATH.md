# Fixed Point Math Guide

## 16.16 Fixed Point Format

The Saturn has no FPU, so we use 16.16 fixed-point numbers:
- **High 16 bits**: Integer part
- **Low 16 bits**: Fractional part
- **Range**: -32768 to 32767
- **Precision**: ~0.000015

## Constants

```c
#define FIX16_SHIFT 16
#define FIX16_ONE   (1 << 16)  // 65536
#define FIX16_HALF  (1 << 15)  // 32768

// Common conversions
#define FIX16_TO_FLOAT(x) ((float)(x) / 65536.0f)
#define FLOAT_TO_FIX16(x) ((fix16_t)((x) * 65536.0f))
```

## Basic Operations

### Addition/Subtraction
```c
fix16_t a = FLOAT_TO_FIX16(1.5f);
fix16_t b = FLOAT_TO_FIX16(2.5f);
fix16_t result = a + b;  // 4.0
```

### Multiplication
```c
fix16_t result = fix16_mul(a, b);

// Internally uses assembly:
// dmuls.l r4, r5
// xtrct r5, r4, r0
```

### Division
```c
fix16_t result = fix16_div(a, b);
```

## Assembly Optimized Multiply

The SH-2 `dmuls.l + xtrct` instruction provides 64-bit precision:

```asm
.global fix16_assembly_mul
fix16_assembly_mul:
    dmuls.l r4, r5    // 64-bit multiply
    xtrct r5, r4, r0   // Extract middle 32 bits
    rts
    nop
```

## Usage Examples

### Rotation
```c
fix16_t angle = FLOAT_TO_FIX16(0.785f);  // 45 degrees
Mat4 rotation;
mat4_rotate_y(&rotation, angle, &rotation);
```

### Vector Transform
```c
Vec3 position = {FIX16_ONE, FIX16_ONE >> 1, FIX16_ONE >> 2};
Vec3 transformed;
vec3_transform(&position, &rotation, &transformed);
```

### Scale
```c
fix16_t scale = FLOAT_TO_FIX16(2.0f);
Vec3 scaled;
vec3_mul(&position, scale, &scaled);
```

## Performance Tips

1. **Use assembly for multiplication** - `fix16_assembly_mul()`
2. **Precompute common constants** - Don't convert floats at runtime
3. **Batch operations** - Transform multiple vertices together
4. **Avoid division** - Use multiplication by reciprocal when possible

## Common Pitfalls

### ❌ Mixing integer and fixed-point
```c
fix16_t a = FIX16_ONE;
int b = 2;
fix16_t result = a * b;  // WRONG - integer math
```

### ✅ Always use fixed-point operations
```c
fix16_t a = FIX16_ONE;
fix16_t b = FLOAT_TO_FIX16(2.0f);
fix16_t result = fix16_mul(a, b);  // CORRECT
```

### ❌ Overflow with large numbers
```c
fix16_t a = FLOAT_TO_FIX16(40000.0f);  // OVERFLOW
```

### ✅ Keep values in range
```c
fix16_t a = FLOAT_TO_FIX16(10000.0f);  // Safe range
```

## Debugging

### Print fixed-point values
```c
printf("Value: %.4f (0x%08X)\n", 
       FIX16_TO_FLOAT(value), value);
```

### Check for overflow
```c
if ((value >> 16) > 32767 || (value >> 16) < -32768) {
    // Overflow occurred
}
```
