# Compact projected face queue

`sat_scene3d_faces_use_projected_storage` binds an optional 24-byte SH-2
record for the common projected opaque RGB/compiled-material path. Previously,
each accepted face wrote and later reread a 104-byte general record. Clipped,
Gouraud and other materials retain an owned copy in ordinary cold storage.
The existing stable global painter order and texture-owner checks still apply.

Both buffers are caller-owned and must cover the scene capacity. Bind between
frames, retain resources through flush, and use `sat_scene3d_faces_get` for
diagnostics instead of directly indexing `entries`. The compact queue adds
24 bytes per capacity slot and retains worst-case general storage. Ordinary
batches still copy on merge; direct spans described below remove those copies. Dino enables it by default; set
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


## Direct worker spans

Bind a `uint16_t[capacity]` logical-to-physical map with
`sat_scene3d_faces_use_projected_indices` between frames. Initialize a batch
normally (the initial face pointer can be the canonical cold storage), then call
`sat_scene3d_faces_reserve_batch` after scene begin and before async dispatch.
The reservation replaces its face pointers with disjoint slices of the canonical
projected and cold queues. Batch key storage remains separate.

Master submissions may proceed while the worker prepares the reserved slice.
After completion, `sat_scene_merge_prepared_batch` copies only painter keys and
publishes logical indices; it never copies projected or cold face records.
Logical submission order is prefix, Master work, then merged worker work, even
when physical storage has a different order. The existing stable painter sorts
those logical indices once. `sat_scene3d_faces_get`, command-budget admission,
texture validation and emission resolve the same map.

Only one unmerged reservation per scene is supported. Reserve worst-case face
capacity, including faces that might be culled; unused slots remain unavailable
until the next begin. A new reservation after a completed merge is allowed if
physical and cold capacity remain. This explicit bound avoids allocation,
compaction and synchronization between concurrent writers. The scene refuses to
flush while a reservation is outstanding. Do not copy or reinitialize a scene or
batch holding a reservation.

A completed batch can release its task and reuse its keys/descriptor after
merge: all face storage belongs to the canonical queue and remains valid through
flush. Releasing an unmerged async batch cancels the reservation only after the
executor confirms release. A timeout retains ownership. For a synchronous or
not-yet-dispatched batch, `sat_scene3d_faces_cancel_batch` abandons its reservation;
it returns BUSY while the async task is pending. If submission fails, cancel the
reservation explicitly. Cancellation retains unused capacity until next begin.

The worker writes its projected output and cold fallbacks through uncached
aliases; completion invalidates projected output on the consumer, and the adapter
synchronizes keys and cold ranges. Material/source publication and generation
checks retain their existing contracts. No texture upload or VDP1 emission occurs
on the worker.

Host tests exercise mixed projected/clipped/culled faces, equal-depth order,
appending during and after a reservation, exact ordinary-queue equivalence,
empty batches, capacity failure, duplicate merge rejection and cancellation.
Adapter tests verify projected output size, cold/key synchronization, Master
fallback and ownership retention on failed release. The port qualification is
recorded in its `docs/STRUCTURAL_PERFORMANCE_V2.md`.


An initial direct-span prototype regressed Dino's unchanged contiguous queue
from 23.9 to 20.0 submissions/s. Matched Ymir CPU profiles isolated additional
queue validation/emission and command-budget overhead. Dispatching once per queue
between contiguous and indexed readers restored the animated result to 23.9/s;
the contiguous path no longer tests the index-map pointer for every face.
Do not qualify only the consumer that enables the new feature.
