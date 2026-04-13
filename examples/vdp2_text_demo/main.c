#include <stdint.h>
#include <stddef.h>

#include "saturn/saturn.h"
#include "saturn/example_util.h"

// 4-color palette for text display
static const uint16_t text_palette[4] = {
    0x0000,  // Black (background)
    0x7FFF,  // White (text)
    0xFC00,  // Red (highlight)
    0x83E0   // Green (accent)
};

// Text buffer for NBG0 (40x28 characters for 320x224 screen)
static uint8_t text_buffer[40 * 28];
static uint16_t font_tiles_4bpp[96 * 16];
static uint16_t text_pattern_names[40 * 28];
static const uint32_t kNbg0CharBaseWordOffset = 0u;

static void clear_text_buffer(void) {
    for (int i = 0; i < 40 * 28; ++i) {
        text_buffer[i] = 0;  // Space character
    }
}

static void put_char(int x, int y, char ch) {
    if (x >= 0 && x < 40 && y >= 0 && y < 28) {
        const int code = (unsigned char)ch;
        if (code < 32 || code > 127) {
            text_buffer[y * 40 + x] = 0u;
        } else {
            text_buffer[y * 40 + x] = (uint8_t)(code - 32);  // Convert ASCII to font index
        }
    }
}

static void put_string(int x, int y, const char* str) {
    int cx = x;
    while (*str) {
        if (*str == '\n') {
            cx = x;
            y++;
        } else {
            put_char(cx, y, *str);
            cx++;
        }
        str++;
    }
}

static sat_result_t upload_font_to_vram(void) {
    for (uint32_t tile = 0; tile < 96u; ++tile) {
        const uint32_t tile_word_base = tile * 16u;
        const uint8_t* font_rows = sat_font_ascii_8x8_rows((char)(tile + 32u));
        for (uint32_t row = 0; row < 8u; ++row) {
            const uint8_t bits = font_rows[row];
            uint16_t left_word = 0u;
            uint16_t right_word = 0u;

            for (uint32_t col = 0; col < 4u; ++col) {
                const uint16_t pixel = (uint16_t)((bits >> (7u - col)) & 1u);
                left_word = (uint16_t)(left_word | (uint16_t)(pixel << (12u - (col * 4u))));
            }
            for (uint32_t col = 4u; col < 8u; ++col) {
                const uint16_t pixel = (uint16_t)((bits >> (7u - col)) & 1u);
                right_word = (uint16_t)(right_word | (uint16_t)(pixel << (12u - ((col - 4u) * 4u))));
            }

            font_tiles_4bpp[tile_word_base + (row * 2u)] = left_word;
            font_tiles_4bpp[tile_word_base + (row * 2u) + 1u] = right_word;
        }
    }

    SAT_TRY(sat_vdp2_vram_write_words(
        kNbg0CharBaseWordOffset,
        font_tiles_4bpp,
        (uint32_t)(96u * 16u)
    ));

    return SAT_OK;
}

static sat_result_t upload_text_to_vram(void) {
    for (uint32_t y = 0; y < 28u; ++y) {
        for (uint32_t x = 0; x < 40u; ++x) {
            text_pattern_names[y * 40u + x] = text_buffer[y * 40u + x];
        }
    }

    SAT_TRY(sat_vdp2_nbg0_map_fill(0u));

    const sat_vdp2_map_region_t region = {0u, 0u, 40u, 28u};
    SAT_TRY(sat_vdp2_nbg0_map_write_region(text_pattern_names, &region, 40u));

    return SAT_OK;
}

int main(void) {
    // Initialize Saturn
    sat_video_config_t cfg = {320, 224, 1, 0};
    sat_example_must(sat_init(&cfg));
    
    const sat_vdp2_nbg0_config_t nbg0_cfg = {
        SAT_VDP2_CHAR_SIZE_1X1,
        SAT_VDP2_COLOR_MODE_16,
        0x0010u,
        1u,
        0u
    };
    sat_example_must(sat_vdp2_nbg0_init(&nbg0_cfg));
    const sat_vdp2_scroll_t scroll = {0u, 0u, 0u, 0u};
    sat_example_must(sat_vdp2_nbg0_set_scroll(&scroll));
    
    // Upload palette for text
    sat_example_must(sat_vdp2_palette_upload(text_palette, 4, 0));
    
    // Upload font to VRAM
    sat_example_must(upload_font_to_vram());
    
    // Clear text buffer and display welcome message
    clear_text_buffer();
    put_string(5, 5, "HELLO WORLD!");
    put_string(5, 7, "VDP2 TEXT DEMO");
    put_string(5, 9, "USING NBG0 FOR");
    put_string(5, 11, "TEXT DISPLAY");
    put_string(5, 13, "PRESS START TO EXIT");
    
    // Upload text to VRAM
    sat_example_must(upload_text_to_vram());
    
    // Enable NBG0
    sat_example_must(sat_vdp2_nbg0_set_enabled(1));
    
    // Set backdrop color
    sat_example_must(sat_vdp2_back_color_set(0x001F));  // Blue background
    
    // Main loop
    uint32_t frame_counter = 0;
    for (;;) {
        sat_pad_state_t pad = {0};
        sat_example_must(sat_wait_vblank());
        sat_example_must(sat_pad_poll(&pad));
        
        // Exit on START button
        if ((pad.pressed & SAT_PAD_START) != 0u) {
            break;
        }
        
        // Update display every 30 frames
        if ((frame_counter % 30) == 0) {
            // Flash the backdrop color
            uint16_t bg_color = ((frame_counter / 30) & 1) ? 0x001F : 0x03E0;
            sat_example_must(sat_vdp2_back_color_set(bg_color));
        }
        
        ++frame_counter;
    }
    
    return 0;
}
