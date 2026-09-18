# Public API ownership and capacity

This is the ownership contract for the high-level runtime surface. Public
runtime APIs are single-threaded unless a future API explicitly says otherwise;
callers own storage passed as a buffer, arena, pool, atlas, model, or scene
scratch region. The runtime performs no general-purpose heap allocation.

| Resource | Caller owns | Runtime owns | Lifetime / capacity rule | Hardware boundary |
| --- | --- | --- | --- | --- |
| Core/video state | configuration value | fixed global runtime state | one initialized runtime; `sat_shutdown` invalidates handles | VDP1/VDP2 setup is behind `core`, `video`, and `app` |
| Arena/pool memory | backing bytes and capacity | allocation cursors/slot metadata | reset only when dependent objects are released | no hardware state |
| Surface | pixel/storage bytes when caller-backed | descriptor and generation | caller storage outlives the surface; fixed surface slots | format/layout validation is centralized |
| Logical texture | source bytes and optional update buffer | logical descriptor and generation | source remains valid while registered; destroy invalidates handle | gameplay does not manage VDP1/CRAM |
| Native VDP1 texture | upload source and destination policy | native descriptor/VRAM cursor | fixed VRAM budget; no arbitrary VRAM compaction | explicitly low-level `sat_vdp1_*` API |
| Texture region | region handle metadata | resolver/cache entries | fixed region capacity; generation invalidation; prewarm can fail deterministically | source-address/stride math stays in texture runtime |
| Font atlas | atlas pixels and glyph metadata | font descriptor | caller-backed atlas/metadata remain valid until destroy | baked atlas only; no runtime font decode |
| Sound | PCM sample bytes | bounded sound descriptor/player state | source remains valid while registered; fixed sound slots | SCSP programming stays in audio layer |
| Audio stream/music | ring storage and staging buffers | stream state and bounded scheduler state | fixed stream count; close invalidates handle; writes reject overrun | SCSP/CD transport is hidden behind adapters |
| File handle | backend context and mount table storage | fixed mount/handle records | generation-checked handles; close releases one slot | backend may be host file, memory, or CD/CDFS |
| Logical asset | manifest/source descriptors | fixed registry records | generation-checked handles; source remains caller-owned | physical assets stay non-resident until read |
| CDFS/CD device | sector buffer and device context | parser/device adapter state | caller keeps device and destination storage alive | `sat_cd_block_*` is synchronous; BIOS owns authentication |
| Camera/scene/model | camera, transforms, sort/vertex/face scratch | draw-time validation only | caller scratch remains valid for the draw/scene lifetime | scene facade owns projection/command encoding |
| Input/events | output state and event destination | two-port state and bounded event queue | queue drops oldest deterministically on overflow | SMPC polling is isolated from gameplay |

Capacity failures return `SAT_ERR_CAPACITY`; stale or closed generation-checked
handles return the documented invalid/not-found result. Update operations do
not silently reallocate or move caller-owned storage. Hardware-facing failures
are reported as `SAT_ERR_BUSY`, `SAT_ERR_TIMEOUT`, or `SAT_ERR_IO` where the
layer can distinguish them.
