/* vdp2_rbg0_debug.c - Debug RBG0 step by step
 * 
 * This diagnostic tool will:
 * 1. Initialize RBG0 step by step
 * 2. Dump VRAM contents to verify bitmap upload
 * 3. Read back register values
 * 4. Display debug info on screen
 */
#include <stdint.h>

#include "saturn/saturn.h"
#include "saturn/example_util.h"
#include "vdp2_rbg0_debug/seamless_sea.h"

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 224

#define BITMAP_WIDTH  512
#define BITMAP_HEIGHT 256

#define BITMAP_BASE_WORD  0x00000u
#define ROT_PARAM_BASE    0x10000u

/* VRAM access */
#define VRAM16 ((volatile uint16_t*)(0x20000000u | 0x05E00000u))
#define VDP2_REG(addr) (*(volatile uint16_t*)(0x20000000u | 0x05F80000u | (addr)))

/* Debug text rendering using VDP1 system fonts */
static void debug_text(const char* str, int x, int y, uint8_t color) {
    /* Simple placeholder - we'll use VDP1 later */
    (void)str;
    (void)x;
    (void)y;
    (void)color;
}

int main(void) {
    sat_video_config_t cfg = {SCREEN_WIDTH, SCREEN_HEIGHT, 1, 0};
    sat_example_must(sat_init(&cfg));

    /* Step 1: Upload palette */
    sat_vdp2_palette_upload(seamless_sea_asset.palette, 256, 0);
    
    /* Step 2: Upload bitmap data */
    {
        uint32_t word_offset = BITMAP_BASE_WORD;
        const uint8_t* pixels = seamless_sea_asset.pixels;
        uint32_t src_width = seamless_sea_asset.width;
        uint32_t src_height = seamless_sea_asset.height;
        
        for (uint32_t y = 0; y < BITMAP_HEIGHT; ++y) {
            for (uint32_t x = 0; x < BITMAP_WIDTH; x += 2) {
                uint32_t src_x = x % src_width;
                uint32_t src_y = y % src_height;
                
                uint16_t word = 0;
                word = (uint16_t)((pixels[src_y * src_width + src_x] << 8) |
                                  (src_x + 1 < src_width ? 
                                   pixels[src_y * src_width + (src_x + 1) % src_width] : 0));
                VRAM16[word_offset++] = word;
            }
        }
    }
    
    /* Step 3: Verify bitmap upload - check first and last few words */
    uint16_t first_words[4];
    uint16_t last_words[4];
    
    for (int i = 0; i < 4; i++) {
        first_words[i] = VRAM16[BITMAP_BASE_WORD + i];
        last_words[i] = VRAM16[BITMAP_BASE_WORD + 65532 + i];  /* 65536 total words */
    }
    
    /* Step 4: Setup rotation parameters */
    {
        uint32_t base = ROT_PARAM_BASE;
        
        /* Clear all params */
        for (uint32_t i = 0; i < 60; ++i) {
            VRAM16[base + i] = 0x0000u;
        }
        
        /* Set dX=1.0, dY=1.0 */
        VRAM16[base + 10] = 0x0001u;
        VRAM16[base + 12] = 0x0001u;
        
        /* Set identity matrix */
        VRAM16[base + 14] = 0x0001u;  /* A */
        VRAM16[base + 22] = 0x0001u;  /* E */
        VRAM16[base + 30] = 0x0001u;  /* I */
        
        /* Set scaling */
        VRAM16[base + 42] = 0x0001u;  /* kx */
        VRAM16[base + 44] = 0x0001u;  /* ky */
    }
    
    /* Step 5: Read rotation params to verify */
    uint16_t rot_params[20];
    for (int i = 0; i < 20; i++) {
        rot_params[i] = VRAM16[ROT_PARAM_BASE + i];
    }
    
    /* Step 6: Configure RBG0 via HAL */
    const sat_vdp2_rbg0_config_t rbg0_cfg = {
        SAT_VDP2_RBG0_BITMAP_512x256,
        SAT_VDP2_COLOR_MODE_256,
        BITMAP_BASE_WORD,
        ROT_PARAM_BASE
    };
    sat_vdp2_rbg0_init(&rbg0_cfg);
    sat_vdp2_rbg0_set_param_mode(SAT_VDP2_RBG0_PARAM_A);
    sat_vdp2_rbg0_set_coordinate_increments(ROT_PARAM_BASE, 1, 0, 1, 0);
    sat_vdp2_rbg0_set_scroll(ROT_PARAM_BASE, 0, 0, 0, 0);
    
    /* Step 7: Read VDP2 registers for debug */
    uint16_t reg_bgon   = VDP2_REG(0x020);  /* BGON */
    uint16_t reg_chctlb = VDP2_REG(0x02C);  /* CHCTLB */
    uint16_t reg_ramctl = VDP2_REG(0x00E);  /* RAMCTL */
    uint16_t reg_rptau  = VDP2_REG(0x0BC);  /* RPTAU */
    uint16_t reg_rptal  = VDP2_REG(0x0BE);  /* RPTAL */
    uint16_t reg_bmpnb  = VDP2_REG(0x02E);  /* BMPNB */
    uint16_t reg_rpmd   = VDP2_REG(0x0B0);  /* RPMD */
    
    /* Step 8: Enable RBG0 */
    sat_vdp2_rbg0_set_enabled(1);
    
    /* Step 9: Read BGON again */
    uint16_t reg_bgon_after = VDP2_REG(0x020);
    
    /* Step 10: Set backdrop */
    sat_vdp2_back_color_set(0x0000);
    
    /* Main loop - display debug info */
    /* For now, just halt and let us check via emulator memory viewer */
    while (1) {
        sat_pad_state_t pad = {0};
        sat_wait_vblank();
        sat_pad_poll(&pad);
        
        if ((pad.pressed & SAT_PAD_START) != 0u) {
            break;
        }
        
        /* Toggle RBG0 on/off with A button for testing */
        if ((pad.pressed & SAT_PAD_A) != 0u) {
            static int enabled = 1;
            enabled = !enabled;
            sat_vdp2_rbg0_set_enabled(enabled);
        }
    }
    
    return 0;
}
