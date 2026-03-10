# API C do MVP

Header publico: `include/saturn/saturn.h`.

## Ciclo de vida

- `sat_init(const sat_video_config_t* config)`
- `sat_shutdown(void)`

## Loop de frame

- `sat_wait_vblank(void)`
- `sat_begin_frame(void)`
- `sat_end_frame(void)`

## Input

- `sat_pad_poll(sat_pad_state_t* out_state)`
- `sat_pad_held(void)`

## Render 2D (VDP1)

- `sat_tex_upload_indexed8(...)`
- `sat_draw_sprite(const sat_sprite_cmd_t* cmd)`
- `sat_set_clear_color(uint16_t rgb555)`

## Convencoes

- Coordenadas em `sat_fx16_t` (16.16 fixed-point).
- Largura de textura deve ser multipla de 8.
- Sem uso de `float` na API publica.
- Retorno por `sat_result_t`.

