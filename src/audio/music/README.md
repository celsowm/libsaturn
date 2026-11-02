# Music internals

The **only public music API** is `include/saturn/music.h`. The registered
asset determines whether the source is PCM8, PCM16, IMA ADPCM, or ADX;
the caller never selects a different player for each codec.

## Module responsibilities

| File | Owns |
| --- | --- |
| `player.cpp` | Two music handles, transport selection, play/pause/seek/stop, standby handoff, lifecycle and stats |
| `open.cpp` | Validation, asset resolution, caller workspace partitioning and SCSP stream acquisition |
| `decoder.cpp` | Synchronous PCM/IMA/ADX feed, planar stereo conversion and SCSP PCM writes |
| `disc_feed.cpp` | Sector requests, bounded in-flight reservations, partial compressed frames and prefetch priming |
| `codecs/ima.hpp` | Stateless IMA block decoding |
| `codecs/adx.hpp` | ADX LPC coefficients, history and frame decoding |
| `internal.hpp` | Private shared state and module contracts (not installed as an SDK header) |

## Invariants

1. Large audio buffers are **caller-owned** and must be 4-byte aligned.
   `SAT_MUSIC_WORKSPACE_BYTES` sizes PCM rings and decode scratch.
2. Two concurrently open music handles must never share workspace memory.
   Handle generations prevent stale references.
3. Both stereo SCSP streams must be written and started in lockstep.
4. CD feeds reserve room for **all pending decoded frames** before requesting
   new sectors; partial ADX/IMA blocks carry across delivery boundaries.
5. Every loop resets ADX predictor history, without outputting padded tail
   samples. Nonzero ADX seek is rejected until predictor checkpoints exist.
6. The primary CD stream and its standby use the **same scheduler**; music
   and data jobs share the drive without the SH-2 blocking for music sectors.
7. No hidden 288 KiB global pool, independent ADX player, compatibility API
   or codec selection in game logic.
8. The SH-2 currently decodes ADPCM; offloading to the 68000 would require
   a separately measured architecture, not just a change of codec.
9. A track on the disc opens only with a disc scheduler (`desc->disc`); without one `sat_music_open` returns
   `SAT_ERR_UNSUPPORTED`. The synchronous reader (`sat_asset_read_at` per ADPCM block) stays for RAM tracks and
   non-disc readers, where a block read is not a drive access.
10. Audio continuity and real-drive seeks require testing in emulator and on
   hardware; passing host tests is not evidence of hardware performance.

## Tests

`make test` runs the host music-API and simulated CD integration tests,
including ADX, stereo PCM, buffer ownership, looping, seeks and handoff.
The cross-target build and examples are covered by GitHub Actions.
Use `make EXAMPLE=adx_cd_music` and `make EXAMPLE=pcm8_cd_music` to
exercise the same API with different encodings.
