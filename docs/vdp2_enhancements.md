# VDP2 HAL Enhancements

This document describes the enhanced VDP2 (Video Display Processor 2) HAL module that has been added to libsaturn, based on the ChibiAkumas assembly tutorial for Sega Saturn text display.

## Overview

The VDP2 HAL has been enhanced to support:
- **NBG0 (Normal Background 0) configuration** for text display
- **Palette management** for VDP2 CRAM
- **VBlank synchronization** functions
- **Screen mode control** (RGB555/RGB888)
- **Scroll register control**
- **Backdrop color management**

## New Functions

### Basic Initialization
```c
void init_ntsc_320x224();
uint16_t read_tvstat();
```

### NBG0 Configuration
```c
void configure_nbg0_character(CharacterSize char_size, ColorMode color_mode);
void set_nbg0_scroll(uint16_t x_integer, uint16_t x_fraction, 
                     uint16_t y_integer, uint16_t y_fraction);
void enable_nbg0(bool enable);
```

### Palette Management
```c
void upload_palette(const uint16_t* palette_rgb555, uint16_t count, uint16_t offset = 0);
void upload_palette_4(const uint16_t palette_rgb555[4], uint16_t offset = 0);
void upload_palette_16(const uint16_t palette_rgb555[16], uint16_t offset = 0);
```

### VBlank Synchronization
```c
void wait_vblank_start();
void wait_vblank_end();
```

### Screen Control
```c
void set_screen_mode(ScreenMode mode);
void set_display_enable(bool enable);
void set_map_offset(uint16_t offset);
```

## Public API (saturn.h)

The public API was refactored (breaking v0). Current VDP2 entry points:

```c
sat_result_t sat_vdp2_nbg0_init(const sat_vdp2_nbg0_config_t* config);
sat_result_t sat_vdp2_nbg0_set_scroll(const sat_vdp2_scroll_t* scroll);
sat_result_t sat_vdp2_nbg0_set_enabled(uint8_t enable);
sat_result_t sat_vdp2_palette_upload(const uint16_t* palette_rgb555, uint16_t count, uint16_t offset);
sat_result_t sat_vdp2_vram_write_words(uint32_t word_offset, const uint16_t* words, uint32_t word_count);
sat_result_t sat_vdp2_nbg0_map_fill(uint16_t pattern_name);
sat_result_t sat_vdp2_nbg0_map_write_region(
    const uint16_t* pattern_names,
    const sat_vdp2_map_region_t* region,
    uint16_t source_stride
);
sat_result_t sat_vdp2_wait_vblank_start(void);
sat_result_t sat_vdp2_wait_vblank_end(void);
sat_result_t sat_vdp2_back_color_set(uint16_t rgb555);
```

## Usage Example

See `examples/vdp2_text_demo/main.c` for a complete example that demonstrates:

1. **Initializing VDP2 for text display**
2. **Configuring NBG0 with 1x1 character size and 16-color mode**
3. **Uploading a 4-color palette**
4. **Displaying text using a built-in font**
5. **Using VBlank synchronization**

### Basic Text Display Setup

```c
// Initialize Saturn
sat_video_config_t cfg = {320, 224, 1, 0};
sat_init(&cfg);

// Configure NBG0 for text display
const sat_vdp2_nbg0_config_t nbg0_cfg = {
    SAT_VDP2_CHAR_SIZE_1X1,
    SAT_VDP2_COLOR_MODE_16,
    0x0010, // map plane index
    1,      // transparency code enabled
    0
};
sat_vdp2_nbg0_init(&nbg0_cfg);

// Upload palette for text
const uint16_t text_palette[4] = {
    0x0000,  // Black (background)
    0xFFFF,  // White (text)
    0xFC00,  // Red (highlight)
    0x83E0   // Green (accent)
};
sat_vdp2_palette_upload(text_palette, 4, 0);

// Enable NBG0
sat_vdp2_nbg0_set_enabled(1);

// Set backdrop color
sat_vdp2_back_color_set(0x001F);  // Blue background
```

## Technical Details

### VDP2 Register Map

The enhanced HAL uses the following VDP2 registers:

| Register | Address | Description |
|----------|---------|-------------|
| TVMD | $25F80000 | TV Mode |
| TVSTAT | $25F80004 | Screen Status |
| RAMCTL | $25F8000E | RAM Control (CRAM mode and VRAM partition) |
| BGON | $25F80020 | Screen Display Enable |
| CHCTLA | $25F80028 | Character Control (NBG0, NBG1) |
| PNCN0 | $25F80030 | NBG0 Pattern Name Control |
| PLSZ | $25F8003A | Plane Size |
| MPOFN | $25F8003C | Map Offset Register |
| MPABN0 | $25F80040 | NBG0 Plane A/B Map Address |
| MPCDN0 | $25F80042 | NBG0 Plane C/D Map Address |
| SCXIN0 | $25F80070 | Screen Scroll Value (Horiz Integer) |
| SCXDN0 | $25F80072 | Screen Scroll Value (Horiz Fraction) |
| SCYIN0 | $25F80074 | Screen Scroll Value (Vert Integer) |
| SCYDN0 | $25F80076 | Screen Scroll Value (Vert Fraction) |
| ZMXIN0 | $25F80078 | Coordinate Increment X (integer) |
| PRISA | $25F800F0 | Sprite Priority Register A |
| PRINA | $25F800F8 | NBG0/NBG1 Priority Register |
| BKTAU | $25F800AC | Back Screen Table Address (upper) |
| BKTAL | $25F800AE | Back Screen Table Address (lower) |

### Character Modes

The CHCTLA register controls NBG0 character display:

- **Character Size**: 1x1 (8x8 pixels) or 2x2 (16x16 pixels)
- **Color Mode**: 16 colors (4bpp), 256 colors (8bpp), 2048 colors (16bpp), etc.

The text demo uses:
- `PNCN0 = 0xC000` (1-word pattern name + 12-bit character number).
- Plane size `1H x 1V` and NBG0 plane A map at VRAM offset `0x8000`.
- Character pattern data at VRAM offset `0x0000` (8x8 4bpp tiles = 32 bytes each).

### Palette Upload

Palettes are uploaded to VDP2 CRAM at $25F00000. The enhanced HAL supports:
- 4-color palettes (for 1bpp text)
- 16-color palettes (for 4bpp graphics)
- Custom palette sizes

### VBlank Synchronization

The TVSTAT register bit 3 indicates VBlank status:
- **wait_vblank_start()**: Waits for VBlank to begin
- **wait_vblank_end()**: Waits for VBlank to end

## Building the Examples

To build the VDP2 text demo:

```bash
# Using PowerShell on Windows
powershell -ExecutionPolicy Bypass -File build-example.ps1 vdp2_text_demo

# Using make on Unix-like systems
make APP_C_SRCS=examples/vdp2_text_demo/main.c all
```

## Future Enhancements

Potential future improvements:
- Support for NBG1, NBG2, NBG3 backgrounds
- Bitmap mode support
- Rotation/zoom capabilities
- Line and cell scroll effects
- Transparency and blending modes

## References

- [ChibiAkumas VDP2 Tutorial](https://www.chibiakumas.com/sh2/helloworld.php)
- Sega Saturn Hardware Manual
- VDP2 Register Documentation
