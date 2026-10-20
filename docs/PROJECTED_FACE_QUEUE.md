# Compact projected face queue

`sat_scene3d_faces_use_projected_storage` binds an optional 24-byte SH-2
record for the common projected opaque RGB/compiled-material path. Previously,
each accepted face wrote and later reread a 104-byte general record. Clipped,
Gouraud and other materials retain an owned copy in ordinary cold storage.
The existing stable global painter order and texture-owner checks still apply.

Both buffers are caller-owned and must cover the scene capacity. Bind between
frames, retain resources through flush, and use `sat_scene3d_faces_get` for
diagnostics instead of directly indexing `entries`. This first increment adds
24 bytes per capacity slot; it does not reduce reserved general storage or
remove worker-to-scene merge copies. Dino enables it by default; set
`DINO_PROJECTED_FACE_QUEUE=0` to retain a matched general-queue control.

## Qualification

Host checks compare program, RGB, Gouraud and clipped entries, painter keys,
emitted order, invalidated textures and fallback lifetime after worker reuse.
The supported host/tools suite and five installed-package checks pass.

Fresh matching full-Paul builds use 556 vertices/773 faces per actor, original
combat/textures, parallel preparation and unchanged resolution. Over 5,400
requested cache-on Ymir fields, the general and compact queues complete
4,041/4,153 ticks and 1,010/1,038 submissions respectively (+2.77% submissions).
A frozen tick-120 comparison has zero changed model pixels. Serial/Slave
geometry verification passes 184 comparisons, and all 24 live combat cases
pass. The general control is about 2% slower than the preceding FRT-only build;
the compact increment must not be described as cost-free for general users.

Independent Mednafen CD movies with isolated neutral gamepads show FRAME
45 to 494 for FRT-only, versus 49 to 538 for the compact queue at seconds
18 and 78: 449 versus 489 submissions in 60 seconds (+8.91%). These are
submission-counter observations, not distinct-picture or physical Saturn FPS.

Matched Dino Ymir runs measure the warmed interval between fields 600 and
1,200:

| Scenario | General submissions/s | Compact submissions/s |
| --- | ---: | ---: |
| Animated | 20.6 | 23.9 |
| Paused, no light | 24.0 | 24.0 |
| Scripted controls | 20.0 | 20.1 |

Runtime gates pass in all scenarios; paused model pixels match exactly. The
30-image/60-logic target for the Paul pair remains unmet. Private evidence is
retained under `harness/build/structural-v2-dino-*` and the sibling port's
`assets/imported/structural-v2-hot-*`; no retail data is published.
