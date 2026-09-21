# raylib Compatibility Layer Plan

Status: design review only. No raylib adapter is implemented in this
repository.

The initial target is a documented subset, not an OpenGL emulation layer.
Every adapter function must lower into an existing Saturn-native primitive or
return a clear unsupported/capacity error.

## Tier 1 — Core and 2D

| raylib concept | LibSaturn target |
|---|---|
| `InitWindow` / `CloseWindow` | `sat_init` / `sat_shutdown` with fixed display config |
| `BeginDrawing` / `EndDrawing` | `sat_app_frame_begin` / `sat_app_frame_end` |
| `WindowShouldClose` | adapter policy over a Saturn input convention |
| `GetTime` / `GetFrameTime` | `sat_time_ms` and unsigned frame deltas |
| `IsKeyDown` / `IsKeyPressed` | `sat_pad_poll_port` and `sat_event_poll` |
| `Image` | caller-backed `sat_surface_t` |
| `Texture2D` | `sat_texture_t` |
| `LoadTexture` | logical `sat_texture_load` |
| `UpdateTexture` | `sat_texture_update` / `sat_texture_update_rect` |
| `DrawTexture*` | `sat_draw_texture` |
| primitive drawing | `sat_fill_rect`, `sat_draw_rect`, `sat_draw_line` |
| `BeginMode2D` / `EndMode2D` | bounded `sat_render2d_push` / `sat_render2d_pop` |
| scissor mode | `sat_render2d_set_clip` |

## Tier 2 — Text, audio, and files

| raylib concept | LibSaturn target |
|---|---|
| `Font` / `LoadFont` | baked `sat_font_t` via `sat_font_load` |
| `MeasureText*` / `DrawText*` | `sat_text_measure` / `sat_text_draw` |
| `Sound` / `LoadSound` | preconverted `sat_sound_t` via `sat_sound_load` |
| `PlaySound` | `sat_sound_play` |
| `Music` / `LoadMusicStream` | `sat_music_t` via `sat_music_open` |
| `UpdateMusicStream` | `sat_music_update` plus `sat_audio_update` |
| file data APIs | `sat_file_*` / `sat_asset_*` |

## Tier 3 — 3D

| raylib concept | LibSaturn target |
|---|---|
| `Camera3D` | `sat_camera3d_t` |
| `BeginMode3D` / `EndMode3D` | `sat_scene3d_begin` / `sat_scene3d_end` |
| `Mesh` / `Model` | generated `sat_model_asset_t` and caller-owned `sat_mesh_t` |
| `DrawModel*` | `sat_scene_submit_instance` with generated `sat_model_asset_t` bindings |
| model animation | existing LibSaturn animation APIs |

## Explicit non-goals

The first adapter must document and reject, rather than emulate:

- GLSL, programmable shaders, and `rlgl` state;
- arbitrary render targets and desktop multi-window behavior;
- unrestricted mouse/cursor/clipboard/monitor APIs;
- runtime PNG/JPEG/TTF/OGG/MP3 decoding;
- semantics that require hidden allocation or unbounded batching.

The native `runtime_2d` and `runtime_3d` examples are the acceptance
references. Adapter implementation should begin only after a concrete source
port demonstrates that these native boundaries are sufficient.
