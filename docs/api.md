# C API for MVP

Public header: `include/saturn/saturn.h`.

## Lifecycle

- `sat_init(const sat_video_config_t* config)`
- `sat_shutdown(void)`

## App Helpers

- `sat_app_init_default(void)`
- `sat_app_frame_begin(uint16_t backdrop_rgb555, uint16_t clear_rgb555, sat_pad_state_t* out_pad)`
- `sat_app_frame_end(void)`

## Frame Loop

- `sat_wait_vblank(void)`
- `sat_begin_frame(void)`
- `sat_end_frame(void)`

## Input

- `sat_pad_poll(sat_pad_state_t* out_state)`
- `sat_pad_held(void)`

## 2D Render (VDP1)

- `sat_tex_upload_indexed8(...)`
- `sat_draw_sprite(const sat_sprite_cmd_t* cmd)`
- `sat_set_clear_color(uint16_t rgb555)`

Recommended flow for assets:

1. Convert the indexed image with `tools/convert_indexed8.py`.
2. Include the generated `*.h` in the example.
3. Call `sat_tex_upload_indexed8()` with `pixels`, `palette`, `width`, `height` and `palette_index` from the generated asset.
4. If the original image is larger than the target sprite, use `--resize` in the converter to generate a version compatible with the VDP1 path.

## Error helpers

- `SAT_TRY(expr)`
- `SAT_PANIC_IF_ERROR(expr)`

Usage example:

```c
#include "text_sprite/sonic_head.h"

sat_texture_t tex = {0};
sat_tex_upload_indexed8(
    &tex,
    sonic_head_asset.pixels,
    sonic_head_asset.width,
    sonic_head_asset.height,
    sonic_head_asset.palette,
    sonic_head_asset.palette_index
);
```

## VDP2 (breaking v0)

- `sat_vdp2_nbg0_init(const sat_vdp2_nbg0_config_t* config)`
- `sat_vdp2_nbg0_set_scroll(const sat_vdp2_scroll_t* scroll)`
- `sat_vdp2_nbg0_set_enabled(uint8_t enable)`
- `sat_vdp2_palette_upload(...)`
- `sat_vdp2_vram_write_words(...)`
- `sat_vdp2_nbg0_map_fill(...)`
- `sat_vdp2_nbg0_map_write_region(...)`
- `sat_vdp2_back_color_set(uint16_t rgb555)`

## Font helpers

- `sat_ascii_font_t`
- `sat_ascii_font_init_8x8_indexed8(...)`
- `sat_ascii_font_measure_text_indexed8(...)`
- `sat_ascii_font_draw_text_indexed8(...)`
- `sat_ascii_font_draw_text_centered_indexed8(...)`
- `sat_font_ascii_8x8_rows(char c)`
- `sat_font_pack_8x8_glyph_indexed8(...)`
- `sat_font_draw_text_line_indexed8(...)`
- `sat_font_draw_text_ascii_indexed8(...)`
- `sat_font_upload_ascii_8x8_textures_indexed8(...)`

## Conventions

- Coordinates in `sat_fx16_t` (16.16 fixed-point).
- Texture width must be multiple of 8.
- No use of `float` in the public API.
- Return by `sat_result_t`.