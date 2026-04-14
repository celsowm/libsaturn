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

/* ------------------------------------------------------------------ */
/* RBG0 (Rotation Background 0) - Infinite plane support              */
/* ------------------------------------------------------------------ */

// Bitmap size modes for RBG0
enum RBG0BitmapSize {
    RBG0_BITMAP_SIZE_256x256 = 0x00,
    RBG0_BITMAP_SIZE_512x256 = 0x01,
    RBG0_BITMAP_SIZE_1024x256 = 0x02,
    RBG0_BITMAP_SIZE_256x512 = 0x04,
    RBG0_BITMAP_SIZE_512x512 = 0x05,
    RBG0_BITMAP_SIZE_1024x512 = 0x06,
    RBG0_BITMAP_SIZE_1024x1024 = 0x07
};

// Rotation parameter mode
enum RBG0ParamMode {
    RBG0_PARAM_MODE_A = 0x00,  /* Use rotation parameter A */
    RBG0_PARAM_MODE_B = 0x01,  /* Use rotation parameter B */
    RBG0_PARAM_MODE_COEFF = 0x02,  /* Switch via coefficient table */
    RBG0_PARAM_MODE_WINDOW = 0x03  /* Switch via rotation window */
};

// RBG0 initialization
void configure_rbg0_bitmap(RBG0BitmapSize bitmap_size, ColorMode color_mode,
                           uint32_t bitmap_base_word, uint32_t rot_param_base_word);
void enable_rbg0(bool enable);
void set_rbg0_param_mode(RBG0ParamMode mode);

// Rotation parameter table upload
void upload_rbg0_rotation_params(uint32_t rot_param_word_offset, const uint16_t* params, uint32_t word_count);

// RBG0 scroll via rotation parameters
void set_rbg0_scroll(uint32_t rot_param_word_offset,
                     int32_t xst_int, int32_t xst_frac,
                     int32_t yst_int, int32_t yst_frac);

// Coordinate increments (dX, dY) for bitmap mapping
void set_rbg0_coordinate_increments(uint32_t rot_param_word_offset,
                                     int32_t dx_int, int32_t dx_frac,
                                     int32_t dy_int, int32_t dy_frac);

// Rotation matrix setup (for perspective/3D effects)
void set_rbg0_rotation_matrix(uint32_t rot_param_word_offset,
                              int32_t angle_x, int32_t angle_y, int32_t angle_z);

// Viewpoint and center coordinates.
// Values are interpreted as 16.16 fixed-point and written as integer-only fields.
void set_rbg0_viewpoint(uint32_t rot_param_word_offset,
                        int32_t px, int32_t py, int32_t pz);
void set_rbg0_center(uint32_t rot_param_word_offset,
                     int32_t cx, int32_t cy, int32_t cz);

// Scaling coefficients in 16.16 fixed-point.
void set_rbg0_scaling(uint32_t rot_param_word_offset,
                      int32_t kx, int32_t ky);

}  // namespace saturn::hal::vdp2

#endif
