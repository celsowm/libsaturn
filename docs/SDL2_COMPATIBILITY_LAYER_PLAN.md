# SDL2 Compatibility Layer Plan

Status: design review only. No SDL2 adapter is implemented in this repository.

The adapter must remain a thin semantic translation over the native LibSaturn
runtime. It must not parse desktop assets, construct VDP1 commands, manage
CRAM/VRAM, speak raw SMPC/SCSP registers, or read CD sectors itself.

## Native mapping

| SDL2 concept | LibSaturn target |
|---|---|
| `SDL_GetTicks` | `sat_time_ms` |
| `SDL_Surface` | caller-backed `sat_surface_t` |
| `SDL_CreateTextureFromSurface` | `sat_texture_create_from_surface` or logical `sat_texture_load` |
| `SDL_UpdateTexture` | `sat_texture_update` / `sat_texture_update_rect` |
| `SDL_RenderCopy` | `sat_draw_texture` with a source rectangle |
| `SDL_RenderCopyEx` | `sat_draw_texture` with draw parameters |
| `SDL_RenderFillRect` | `sat_fill_rect` |
| `SDL_RenderDrawLine` | `sat_draw_line` |
| `SDL_PollEvent` | `sat_event_poll` plus adapter event translation |
| controller polling | `sat_pad_poll_port` / `sat_pad_held_port` |
| queued audio | `sat_audio_stream_open` / `sat_audio_stream_write` |
| short audio | `sat_sound_load` / `sat_sound_play` |
| music stream | `sat_music_open` / `sat_music_update` |
| file data | `sat_file_*` / `sat_asset_*` |

## Required adapter policy

- Use fixed adapter tables with explicit `SAT_ERR_CAPACITY` handling.
- Convert SDL rectangles and points once at the boundary; gameplay-facing
  code must remain in the adapter's documented screen/world convention.
- Map unsupported tint/blend/format combinations to a clear adapter error.
- Keep SDL event memory and translated strings caller-owned or fixed-capacity.
- Require Saturn-ready texture, font, sound, and stream assets at build time;
  runtime PNG/TTF/OGG/MP3 decoding is out of scope.
- Preserve the logical asset path when `IMG_Load`/`Mix_Load*` equivalents are
  added, so storage remains behind `sat_asset_*` and `sat_file_*`.

## Gate before implementation

The adapter should not be started until a representative port can compile
against the mappings above without implementing a second renderer, allocator,
asset converter, or audio scheduler. The native `runtime_2d` acceptance
example is the reference for semantics and supported limitations.
