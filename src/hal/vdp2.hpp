#ifndef SATURN_HAL_VDP2_HPP
#define SATURN_HAL_VDP2_HPP

#include <stdint.h>

namespace saturn::hal::vdp2 {

// Screen modes
enum ScreenMode {
    SCREEN_MODE_RGB555 = 0x0000,  // CRAM mode 0 (1024 colors)
    SCREEN_MODE_RGB888 = 0x1000   // CRAM mode 1 (2048 entries, MSB color-calc bit)
};

// Character sizes for NBG0
enum CharacterSize {
    CHAR_SIZE_1x1 = 0x00,  // 1x1 character (8x8 pixels)
    CHAR_SIZE_2x2 = 0x01   // 2x2 character (16x16 pixels)
};

// Color modes for NBG0
enum ColorMode {
    COLOR_MODE_16 = 0x00,   // 16 colors (4bpp)
    COLOR_MODE_256 = 0x01,  // 256 colors (8bpp)
    COLOR_MODE_2048 = 0x02, // 2048 colors (16bpp)
    COLOR_MODE_32768 = 0x03, // 32768 colors (16bpp)
    COLOR_MODE_16770000 = 0x04 // 16.7M colors (24bpp)
};

// Basic initialization
void init_ntsc_320x224();
uint16_t read_tvstat();

// NBG0 configuration
void configure_nbg0_character(CharacterSize char_size, ColorMode color_mode);
void configure_nbg0_text_layout();
void set_nbg0_map_plane_index(uint16_t plane_index);
uint16_t nbg0_map_plane_index();
void set_nbg0_transparent_code_enabled(bool enabled);
void set_nbg0_scroll(uint16_t x_integer, uint16_t x_fraction, 
                     uint16_t y_integer, uint16_t y_fraction);
void enable_nbg0(bool enable);

// Palette management
void upload_palette(const uint16_t* palette_rgb555, uint16_t count, uint16_t offset = 0);
void upload_palette_4(const uint16_t palette_rgb555[4], uint16_t offset = 0);
void upload_palette_16(const uint16_t palette_rgb555[16], uint16_t offset = 0);

// VBlank synchronization
void wait_vblank_start();
void wait_vblank_end();

// Screen control
void set_screen_mode(ScreenMode mode);
void set_display_enable(bool enable);

// Map offset configuration
void set_map_offset(uint16_t offset);
void set_backdrop_color(uint16_t rgb555);
void write_vram_words(uint32_t word_offset, const uint16_t* words, uint32_t word_count);
void fill_vram_words(uint32_t word_offset, uint16_t value, uint32_t word_count);

}  // namespace saturn::hal::vdp2

#endif
