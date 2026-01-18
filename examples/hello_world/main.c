#include "saturn/types.h"
#include "saturn/system.h"
#include "saturn/vdp1.h"
#include "saturn/vdp2.h"
#include "saturn/shared.h"

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 224
#define FONT_WIDTH 8
#define FONT_HEIGHT 8
#define FONT_SPACING 1

typedef struct {
    char ch;
    u8 rows[FONT_HEIGHT];
} Glyph;

static const Glyph kGlyphs[] = {
    { ' ', { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
    { 'D', { 0xFC, 0x82, 0x81, 0x81, 0x81, 0x82, 0xFC, 0x00 } },
    { 'E', { 0xFF, 0x80, 0x80, 0xFE, 0x80, 0x80, 0xFF, 0x00 } },
    { 'H', { 0x81, 0x81, 0x81, 0xFF, 0x81, 0x81, 0x81, 0x00 } },
    { 'L', { 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xFF, 0x00 } },
    { 'O', { 0x7E, 0x81, 0x81, 0x81, 0x81, 0x81, 0x7E, 0x00 } },
    { 'R', { 0xFE, 0x81, 0x81, 0xFE, 0x88, 0x84, 0x82, 0x00 } },
    { 'W', { 0x81, 0x81, 0x81, 0x91, 0x91, 0x91, 0x6E, 0x00 } },
    { '!', { 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x18, 0x00 } },
};

static int string_length(const char* text) {
    int length = 0;
    while (text[length] != '\0') {
        length++;
    }
    return length;
}

static const Glyph* find_glyph(char ch) {
    for (u32 i = 0; i < (sizeof(kGlyphs) / sizeof(kGlyphs[0])); i++) {
        if (kGlyphs[i].ch == ch) {
            return &kGlyphs[i];
        }
    }
    return &kGlyphs[0];
}

static void draw_pixel(int x, int y, u16 color) {
    if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT) {
        return;
    }
    volatile u16* fb = (volatile u16*)VDP2_VRAM;
    fb[y * SCREEN_WIDTH + x] = color;
}

static void draw_glyph(int x, int y, const Glyph* glyph, u16 color) {
    for (int row = 0; row < FONT_HEIGHT; row++) {
        u8 bits = glyph->rows[row];
        for (int col = 0; col < FONT_WIDTH; col++) {
            if (bits & (0x80 >> col)) {
                draw_pixel(x + col, y + row, color);
            }
        }
    }
}

static void draw_text(int x, int y, const char* text, u16 color) {
    int cursor = 0;
    while (text[cursor] != '\0') {
        draw_glyph(x, y, find_glyph(text[cursor]), color);
        x += FONT_WIDTH + FONT_SPACING;
        cursor++;
    }
}

static void app_slave_main(void) {
    while (1) {
    }
}

static void app_main(void) {
    const char* message = "HELLO WORLD!";
    int message_width = string_length(message) * (FONT_WIDTH + FONT_SPACING) - FONT_SPACING;
    int start_x = (SCREEN_WIDTH - message_width) / 2;
    int start_y = (SCREEN_HEIGHT - FONT_HEIGHT) / 2;

    system_init();
    vdp1_init();
    vdp2_init();

    vdp1_clear_screen(0x0000);
    draw_text(start_x, start_y, message, 0x7FFF);

    while (1) {
        vdp2_wait_for_vblank();
    }
}

void slave_main(void) {
    app_slave_main();
}

void _slave_main(void) {
    app_slave_main();
}

void main(void) {
    app_main();
}

void _main(void) {
    app_main();
}
