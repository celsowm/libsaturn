# LibSaturn Audio & Music Runtime — Execution Plan

## Purpose

Build a complete native audio foundation for LibSaturn before SDL2/raylib compatibility work depends on it.

The end result must support all three common game-audio workloads:

1. **resident sound effects** — short samples such as jump, hit, menu click, explosion;
2. **streamed music / long-form audio** — BGM and long tracks that must not consume the entire SCSP sound RAM;
3. **generic PCM streaming** — procedural audio, decoders, SDL queued audio, raylib `AudioStream`, and future source ports.

This is not a thin wrapper over SCSP registers. LibSaturn must own the difficult Saturn-specific pieces: sound-system startup, sound RAM management, slot/voice scheduling, PCM playback, transfers, buffering, underrun handling, mixing policy, CD/file streaming, and hardware limitations.

The target architecture is:

```text
                       Game / engine
                            |
          +-----------------+-----------------+
          |                 |                 |
      sat_sound_t       sat_music_t     sat_audio_stream_t
     short resident      long/streamed     generic PCM
          |                 |                 |
          +-----------------+-----------------+
                            |
                    High-level audio API
                            |
                 voices / buses / streams
                            |
             sound-RAM manager / scheduler
                            |
                 transport / refill logic
                            |
               +------------+------------+
               |                         |
          SCSP HAL                    CD / file I/O
               |                         |
         slots / mixer / RAM         streamed payload
               |
        SCSP + MC68EC000
```

The future SDL2 and raylib compatibility layers are consumers of this runtime, not architectural templates for it.

---

## Hardware facts that must drive the implementation

Do not encode assumptions from memory. Verify every hardware-sensitive detail against the Sega documentation already vendored under `docs/sega_saturn_hardware`.

At minimum, implementation must account for these documented characteristics:

- Saturn sound block includes the **MC68EC000 sound CPU, SCSP, and 4 Mbits / 512 KiB of sound RAM**;
- SCSP exposes **32 audio slots** usable for PCM/FM;
- waveform data supports at least the documented **8-bit and 16-bit PCM formats**;
- SCSP has per-slot envelope/LFO facilities, output mixing, and DSP facilities;
- SCSP sound-system startup/reset involves the SMPC and the sound CPU;
- SMPC `SNDON`/`SNDOFF` control sound-CPU reset state;
- SCSP has interrupt and DMA facilities, but their real usefulness for our transport path must be measured rather than assumed;
- CD audio/CD data paths have different semantics and must not be conflated with PCM streamed through our runtime.

Before writing constants, cite the corresponding vendored hardware document in code comments where the value is non-obvious or easy to misinterpret.

---

## Harness execution contract

This document is intended to be executed phase by phase by a coding harness.

Rules:

1. Work directly in `celsowm/libsaturn` on `main`.
2. Inspect current code and hardware documentation before implementing each phase.
3. Run host tests after every coherent phase.
4. Build affected examples after every public API change.
5. Preserve the project's **no-runtime-heap** rule.
6. Do not use hidden `malloc`, `new`, unbounded STL containers, or static constructors.
7. Use fixed capacities, caller-owned memory, compile-time storage, or explicit arenas/pools.
8. Keep the low-level SCSP API usable independently of high-level audio.
9. Do not put SDL or raylib types/names into LibSaturn core APIs.
10. Do not parse OGG/MP3/FLAC or other heavyweight desktop codecs on Saturn unless a later benchmark proves that runtime decoding is desirable and bounded.
11. Prefer host-side conversion to a Saturn-friendly payload.
12. Do not claim support for a sample rate, format, stereo mode, loop mode, transfer path, or mixer behavior until it is tested.
13. Capacity failures and underruns must be observable, never silently corrupt audio.
14. Commit coherent, tested phases directly to `main`.
15. Use project build/run/emulator scripts according to `AGENTS.md`.
16. The final example is an acceptance test, not decorative sample code.

---

# Definition of done

This plan is complete only when a normal game can use audio without touching SCSP registers, sound RAM addresses, SMPC sound commands, or transport mechanics.

The final public runtime must provide these distinct concepts:

```text
sat_sound_t
    small/medium reusable sound effect
    typically resident in sound RAM

sat_music_t
    long-form track
    streamed/refilled without requiring full residency

sat_audio_stream_t
    generic PCM producer/consumer stream
    used by decoders, procedural audio and compatibility layers
```

The final `examples/audio_showcase` must demonstrate simultaneously:

- sound-system bring-up;
- one continuously playing/looping music stream;
- multiple overlapping sound effects while music continues;
- at least one mono and one stereo-relevant/panned demonstration where supported;
- per-sound volume;
- per-voice pan;
- master/music/SFX bus volume if the architecture supports buses;
- looping SFX or loop-region behavior;
- pause/resume/stop/restart music;
- a generic PCM stream independent of `sat_music_t`;
- deterministic voice exhaustion/stealing behavior;
- deterministic sound-RAM exhaustion behavior in tests;
- live stream-buffer diagnostics;
- underrun counting;
- no runtime heap allocation;
- stable playback while rendering a normal LibSaturn frame loop;
- build to ISO/CUE and execution through the normal emulator/harness path.

A useful game-facing result should look approximately like:

```c
sat_sound_t jump;
sat_music_t bgm;

SAT_TRY(sat_sound_load(&jump, "audio/jump.wav"));
SAT_TRY(sat_music_open(&bgm, "audio/showcase_theme.ogg"));
SAT_TRY(sat_music_play(bgm, SAT_AUDIO_LOOP));

while (running) {
    sat_audio_update();

    if (pad.pressed & SAT_PAD_A) {
        sat_sound_play(jump, NULL);
    }

    render_frame();
}
```

The physical ISO does not need to contain WAV/OGG files in their desktop representation. Logical path identity may resolve to host-preconverted Saturn payloads.

---

# Phase 0 — Baseline, hardware audit, and architecture decision

Before implementing audio, inspect at minimum:

```text
AGENTS.md
Makefile
build-example.ps1
run-example.ps1
include/saturn/core.h
include/saturn/app.h
include/saturn/input.h
include/saturn/saturn.h
src/core/
src/hal/smpc.cpp
src/hal/smpc.hpp
src/hal/scu.cpp
src/hal/scu.hpp
examples/
tests/host/
harness/
docs/sega_saturn_hardware/hard/scsp/
docs/sega_saturn_hardware/hard/smpc/
docs/sega_saturn_hardware/hard/intr/
```

Record the current baseline:

```text
make test
all standard examples build
normal harness suite result
```

## 0.1 Decide the control architecture by experiment

Do not prematurely commit to one of these models:

```text
A. SH-2-controlled
   SH-2 writes sound RAM / SCSP slot state directly.

B. MC68EC000 driver
   SH-2 sends commands to a resident 68k sound driver.
   The sound CPU owns most slot scheduling/refill work.

C. Hybrid
   SH-2 owns asset/file streaming and command submission;
   68k owns time-sensitive playback/refill/voice work.
```

Create minimal spikes where needed and evaluate:

- implementation complexity;
- CPU cost on SH-2;
- timing stability under a heavy render frame;
- communication overhead;
- refill latency/jitter;
- slot ownership clarity;
- emulator support/reliability;
- feasibility on real Saturn hardware assumptions;
- interaction with CD reads;
- debug/testability.

The plan may begin bring-up with direct SH-2 control because it minimizes variables, but the long-term runtime architecture must be chosen from measurements before high-level APIs harden around it.

Document the chosen design in the code/docs once proven.

Phase gate:

- baseline recorded;
- hardware constants verified;
- control-architecture decision has evidence, not intuition;
- no public high-level API implemented yet.

---

# Phase 1 — Sound-system bring-up and SCSP HAL

Add an explicit SCSP hardware layer, conceptually:

```text
src/hal/scsp.hpp
src/hal/scsp.cpp
```

and any narrowly required SMPC helpers for sound-system reset/startup.

## 1.1 Sound startup

Implement and test the minimal deterministic lifecycle:

```text
system init
   -> establish known sound reset state
   -> initialize/clear required sound RAM/register state
   -> SNDON / release sound CPU as required by chosen architecture
   -> configure SCSP common state
   -> audio ready
```

Shutdown should leave the subsystem in a predictable state and stop active voices.

Do not assume power-on emulator state is representative of hardware.

## 1.2 Low-level SCSP slot primitives

Provide internal/HAL operations sufficient to configure and inspect a slot:

- sample base/start address;
- sample format;
- loop start/end/control;
- pitch/sample-rate parameters;
- direct level/output routing;
- pan;
- envelope values needed for clean PCM playback;
- KEY ON / KEY OFF;
- status needed to detect active/completed playback.

Keep register bit layout in the HAL. Core/high-level code should use semantic structures.

## 1.3 Common/global controls

Implement only what the runtime needs initially:

- master output state/level where appropriate;
- interrupt status/acknowledgement if used;
- mixer routing required for PCM;
- sound-RAM access helpers;
- transport primitives required by measured architecture.

Do **not** implement the SCSP DSP or FM synthesis just because the registers exist.

Phase gate:

- host tests cover pure register encoding logic;
- a deterministic SCSP init sequence exists;
- no magic register values leak above HAL;
- existing non-audio examples still build.

---

# Phase 2 — First PCM: prove one voice end to end

Before building managers, make one known sample play correctly.

Use a tiny repository-generated waveform, not copyrighted audio.

Acceptance fixture:

```text
440 Hz or similarly obvious tone
mono
known sample count
known amplitude
known source sample rate
```

Prove:

- 8-bit PCM if supported by selected first path;
- 16-bit PCM in the next sub-step;
- correct pitch at at least two source sample rates;
- KEY ON starts cleanly;
- KEY OFF stops cleanly;
- one-shot completion behaves correctly;
- looping behaves correctly;
- no stale sound continues across reinitialization.

If an emulator capture or deterministic SCSP state probe can validate frequency/duration automatically, add it to the harness. Otherwise include explicit runtime telemetry and keep the acceptance behavior simple enough to verify reliably.

Do not move to high-level sound objects until this phase is solid.

---

# Phase 3 — Sound RAM allocator and transfer layer

The SCSP has limited sound RAM; treat it as a first-class managed resource.

## 3.1 Deterministic sound-RAM allocator

Create a bounded allocator with explicit alignment rules derived from actual SCSP requirements.

Conceptual API below the public sound layer:

```text
sound_ram_init
sound_ram_alloc
sound_ram_free
sound_ram_stats
```

Requirements:

- no general heap;
- deterministic allocation failure;
- explicit alignment;
- coalescing policy defined if frees are supported;
- stale/double free detectable where practical;
- `used`, `free`, `largest_free`, `high_water` diagnostics;
- reserved regions for driver/streaming buffers are explicit;
- no overlap with 68k program/data if a sound-CPU driver is used.

A fixed block allocator, segregated classes, free-list over static metadata, or another bounded design is acceptable. Choose based on realistic audio asset sizes and fragmentation tests.

## 3.2 Transfer abstraction

Separate **where data comes from** from **how it reaches sound RAM**.

Conceptually:

```text
CPU/file bytes
    |
transfer abstraction
    |
sound RAM
```

Benchmark candidate paths available to the chosen architecture. Do not blindly assume SCSP DMA solves main-CPU-to-sound-RAM transfer; the hardware documentation's DMA restrictions must be respected.

Expose transfer stats useful during the showcase:

```text
bytes uploaded
last transfer bytes
peak transfer time
failed transfers
```

Phase gate:

- allocator stress tests;
- exhaustion test;
- fragmentation/reuse test if frees exist;
- repeated upload/play/free cycles without corruption;
- measured transfer behavior documented.

---

# Phase 4 — Voice manager

A **voice** is a currently playing hardware-backed instance. A **sound** is reusable sample data. Keep those concepts separate.

The SCSP exposes a finite number of slots; the runtime must own scheduling policy.

Conceptual internal voice handle:

```c
typedef struct sat_voice {
    uint8_t slot;
    uint8_t generation;
} sat_voice_t;
```

The exact ABI may differ.

Voice manager responsibilities:

- allocate/free SCSP slots;
- track one-shot completion;
- looping state;
- per-voice volume;
- per-voice pan;
- pitch/playback-rate control where useful;
- pause/resume if implementable cleanly;
- priority;
- reserved slots for streaming/music if needed;
- generation-safe handles;
- deterministic exhaustion behavior.

## 4.1 Voice stealing

Define an explicit policy rather than random slot reuse.

Candidate policy:

1. use a free eligible voice;
2. otherwise choose the lowest-priority stealable voice;
3. break ties by oldest start time;
4. never steal protected music/stream voices unless explicitly allowed.

Allow a play request to opt out of stealing.

Diagnostics:

```text
voices active / capacity
voice steals
failed play requests
per-bus active count
```

Phase gate:

- N overlapping sounds behave deterministically;
- all eligible slots can be exercised in a stress test;
- voice stealing unit tests cover priority/tie behavior;
- stopping one sound does not kill another instance of the same sample.

---

# Phase 5 — High-level resident sound API

Add a game-facing `audio.h` (or split headers only if responsibility remains clearer).

A resident sound is reusable sample data, normally preconverted and uploaded to sound RAM.

Conceptual public type:

```c
typedef struct sat_sound {
    uint16_t slot;
    uint16_t generation;
} sat_sound_t;
```

Do not expose sound-RAM addresses or SCSP slots through this object.

Target operations:

```text
sat_sound_load / create
sat_sound_unload
sat_sound_play
sat_sound_stop_all_instances
sat_sound_info
```

Play parameters should support at least:

```c
typedef struct sat_sound_play_params {
    uint16_t volume;
    int16_t pan;
    uint16_t priority;
    uint16_t flags;
} sat_sound_play_params_t;
```

Final ranges/types should reflect useful fixed-point/hardware resolution rather than copy SDL/raylib types.

Required semantics:

- a `sat_sound_t` may have multiple simultaneous playing voices;
- unloading while active must have a documented policy;
- invalid/stale handles fail safely;
- logical sample rate is preserved by playback pitch calculation;
- loop metadata is explicit;
- SFX bus/master gain interaction is deterministic.

Phase gate:

- load/play/unload cycle;
- overlapping instances;
- pan left/center/right;
- multiple volume levels;
- looping effect;
- stale handle tests;
- deterministic resource exhaustion.

---

# Phase 6 — Audio buses and gain model

Introduce a small game-oriented gain hierarchy if it can be implemented cleanly:

```text
MASTER
  |
  +-- SFX
  |
  +-- MUSIC
  |
  +-- STREAM / optional AUX
```

Required operations may be conceptually:

```text
sat_audio_set_master_volume
sat_audio_bus_set_volume
sat_audio_bus_get_volume
```

Do not emulate an arbitrary desktop mixer graph.

Specify:

- gain representation;
- clamping;
- whether gain changes affect already-playing voices immediately;
- pan law;
- interaction between source gain, voice gain, bus gain, and master gain;
- hardware vs software application of gain.

Avoid expensive per-sample SH-2 mixing when SCSP hardware can perform the needed level/pan operation directly.

Phase gate: gain math host tests and audible/runtime verification in the showcase scaffold.

---

# Phase 7 — Generic PCM streaming core

This is the central primitive for long music, procedural audio, SDL queued audio, and raylib streams.

Conceptual public configuration:

```c
typedef enum sat_audio_format {
    SAT_AUDIO_S8,
    SAT_AUDIO_S16
} sat_audio_format_t;

typedef struct sat_audio_spec {
    uint32_t sample_rate;
    uint32_t buffer_frames;
    uint8_t channels;
    uint8_t format;
} sat_audio_spec_t;
```

Target operations:

```text
sat_audio_stream_open
sat_audio_stream_write
sat_audio_stream_available
sat_audio_stream_buffered
sat_audio_stream_pause
sat_audio_stream_resume
sat_audio_stream_flush
sat_audio_stream_close
sat_audio_stream_stats
```

## 7.1 Buffering architecture

Use a bounded ring/double-buffer strategy with explicit ownership.

Conceptually:

```text
producer
   |
   v
CPU-side bounded ring / staging
   |
refill scheduler
   |
SCSP sound-RAM playback buffers
   |
SCSP voice(s)
```

If the chosen 68k/hybrid architecture moves refill work to the sound CPU, keep the public semantics identical.

## 7.2 Underrun model

Underruns must be observable.

Track at least:

```text
underrun_count
overrun/rejected_write_count
bytes/frames queued
minimum observed fill
maximum observed fill
```

Specify what audio is produced during underrun: silence, held sample, stop/restart, or another documented behavior.

## 7.3 Stereo

Determine experimentally the cleanest stereo strategy under the SCSP architecture. This may use paired voices/buffers or another verified path.

Do not advertise stereo streams until simultaneous-channel synchronization is stable.

Phase gate:

- long generated PCM stream runs for several minutes in emulator without drift/crash;
- deliberate producer starvation increments underrun count predictably;
- pause/resume does not corrupt buffer state;
- ring-buffer logic has pure host tests;
- stereo, if supported, survives long-run synchronization test.

---

# Phase 8 — Music runtime

Build `sat_music_t` **on the generic streaming core**, not as a separate audio engine.

A music object owns source/decoder/stream state but ultimately feeds `sat_audio_stream_t`.

Target operations:

```text
sat_music_open
sat_music_play
sat_music_pause
sat_music_resume
sat_music_stop
sat_music_seek          if source format/backend supports it cleanly
sat_music_set_loop
sat_music_update
sat_music_close
sat_music_info
sat_music_stats
```

`sat_music_update()` may remain explicit in the first version if the game must periodically feed data. If later architecture can make refill autonomous, preserve compatibility with the explicit call as a harmless/service operation.

## 8.1 Looping

Support at least full-track looping.

Later optional support:

- loop start/end metadata;
- intro + loop-body arrangement;
- sample-accurate loop points where the physical format permits it.

Do not fake seamless looping if the CD/filesystem/refill path produces a gap. Measure and test it.

## 8.2 Seek

Seek is useful but not required for the first acceptance if implementing it would destabilize streaming. If deferred, return `SAT_ERR_UNSUPPORTED` explicitly.

Phase gate:

- music loops while gameplay/render loop runs;
- SFX can overlap music without corruption;
- pause/resume/stop/restart stable;
- track restart does not leak stream or sound-RAM resources;
- at least a multi-minute stress playback run.

---

# Phase 9 — Host-side audio asset pipeline

Desktop source formats are build inputs, not necessarily runtime formats.

The intended pipeline is:

```text
WAV / OGG / FLAC / other supported source
              |
          host tooling
              |
      decode / normalize
      resample if required
      channel conversion
      optional compression choice
      loop metadata extraction
              |
       Saturn audio payload
              |
      ISO + asset metadata
```

## 9.1 SFX conversion

For resident effects, generate a compact payload containing at least:

```text
sample format
sample rate
channels / layout
sample count
loop metadata
PCM bytes
```

Prefer direct SCSP-friendly PCM initially. Add compression only when the decode/space tradeoff is measured.

## 9.2 Music conversion

Choose a stream format based on:

- CD bandwidth;
- work-RAM footprint;
- sound-RAM buffer footprint;
- SH-2/68k decode cost;
- seek complexity;
- loop quality;
- source-port usefulness.

The first robust implementation may simply stream preconverted PCM if capacity/bandwidth measurements permit it. Do not introduce ADPCM solely because it sounds more sophisticated.

Evaluate compression as a later measured optimization.

## 9.3 Logical paths

Integrate with the high-level asset/VFS direction:

```c
sat_sound_load(&sound, "audio/jump.wav");
sat_music_open(&music, "audio/theme.ogg");
```

may resolve to generated Saturn-native files/registry entries while preserving the logical source path.

This is important for future SDL/raylib source ports.

## 9.4 Reproducibility

Asset conversion must be deterministic. Tests should verify generated metadata and hashes/byte sizes for repository-owned fixtures where appropriate.

---

# Phase 10 — File/CD streaming integration

Music playback must coexist with actual game I/O.

Build on the repository's eventual high-level file/VFS abstraction when available. Until then, keep the music source interface narrow enough that the backend can be replaced without changing audio semantics.

Requirements:

- partial reads;
- refill-sized reads, not full-file allocation;
- bounded staging buffers;
- EOF and loop restart handling;
- read errors propagated to music state/stats;
- no assumption that file reads complete instantly;
- avoid long blocking refill inside latency-sensitive render paths where possible.

Stress scenarios:

```text
music streaming + rapid SFX
music streaming + normal rendering
music streaming + unrelated asset read
music looping at EOF + SFX burst
intentional delayed refill
```

If CD scheduling becomes a bottleneck, document the policy and expose enough telemetry to diagnose it before inventing asynchronous complexity.

---

# Phase 11 — Update/service model

Define one obvious rule for games:

```c
sat_audio_update();
```

or prove that no global update is required.

If retained, `sat_audio_update()` should perform bounded service work such as:

- reap completed voices;
- process music/stream refills;
- apply deferred commands;
- update diagnostics;
- process sound-CPU mailbox messages if the architecture uses them.

Requirements:

- bounded maximum work per call;
- no file decode of unbounded size;
- safe to call once per game frame;
- behavior under frames slower than 60 Hz documented;
- audio timing must not rely on exactly one call per VBlank.

The audio system must continue correctly when a game frame spans multiple VBlanks, subject to documented buffering limits.

---

# Phase 12 — `examples/audio_showcase`

Create a new first-class example:

```text
examples/audio_showcase/
    main.c
    Makefile.inc
    assets/
        ... source fixtures or generated-input declarations
```

Add any required host generator/converter support under `tools/` or the repository's established asset pipeline.

The example should look and behave like a small **audio test console**, not a blank screen that plays a beep.

## 12.1 Visual HUD

Use existing LibSaturn text/2D facilities to display live state, for example:

```text
LIBSATURN AUDIO SHOWCASE

MUSIC   PLAYING   LOOP ON
TRACK   showcase_theme
TIME    00:42

SFX     laser
PAN     +00
VOL     100%

VOICES  07 / 32
SND RAM 214K / 512K
STREAM  68% full
UNDERRUNS 0
STEALS     3

A   play selected SFX
B   burst / overlap test
C   toggle SFX loop
X/Y select SFX
L/R pan left/right
UP/DOWN volume
Z   generic stream demo
START pause/resume music
```

Exact controls may be adjusted to match existing input conventions, but all major capabilities must be directly exercisable.

## 12.2 Required audio fixtures

Do not depend on copyrighted music.

Generate repository-owned deterministic audio fixtures host-side, such as:

- short sine/pluck tone;
- short noise/percussion effect;
- short frequency sweep;
- stereo/pan-identifiable effect;
- a synthetic multi-second/multi-minute showcase music track generated from simple oscillators/chords/rhythm;
- a generic procedural PCM stream fixture.

A script such as:

```text
tools/generate_audio_showcase_assets.py
```

may create WAV source fixtures deterministically before the normal conversion pipeline consumes them.

The generated BGM should be long enough that streaming is genuinely required or its payload should be repeated/structured so the runtime path cannot accidentally pass by loading the complete music track into sound RAM.

## 12.3 Interactive tests

The example must demonstrate:

### A — single SFX

Play the selected sound once.

### B — burst/voice stress

Trigger a controlled burst that causes overlapping voices and eventually exercises voice-stealing policy without destabilizing music.

### C — loop

Toggle loop on a selected effect or dedicated looping ambience sample.

### X/Y — selection

Cycle among at least 3 effects with visibly/audibly different properties.

### L/R — pan

Move the next/current suitable effect left/right.

### Up/Down — volume

Adjust SFX or selected voice volume using documented increments.

### Start — music pause/resume

Pause and resume streamed music without reallocating the entire audio system.

### Z — generic stream

Toggle a procedurally generated stream so the example proves `sat_audio_stream_t` independently from `sat_music_t`.

Optionally use another button combination for music restart/loop toggle if controls remain understandable.

## 12.4 Stress mode

Include an optional deterministic stress mode that runs without manual button mashing:

```text
- music remains streaming;
- periodic SFX bursts;
- pan alternates;
- volumes vary in a fixed sequence;
- unrelated file reads may be triggered if file API is ready;
- counters remain on-screen.
```

The harness should be able to enter stress mode through deterministic input and run it for a fixed duration.

## 12.5 Acceptance telemetry

Expose at least:

```text
active voices
voice capacity
voice steal count
failed play count
sound RAM used/capacity/high-water
stream buffered frames or percentage
stream underrun count
stream rejected/overrun writes
music state
music loop count
bytes read for music
last audio error
```

If a 68k driver is used, also expose mailbox/driver health counters useful for diagnosing stalls.

---

# Phase 13 — Automated and host testing

## Host tests

Keep pure logic independently testable:

- pitch/sample-rate conversion math;
- volume/gain clamping;
- pan mapping;
- sound-RAM allocator;
- allocator fragmentation/reuse;
- voice allocation;
- voice stealing priority/tie rules;
- generation/stale handles;
- stream ring-buffer wraparound;
- underrun/overrun accounting;
- music state machine;
- loop state;
- asset metadata parsing;
- bounded mailbox logic if using 68k driver.

## HAL/register tests

Verify exact encoding for:

- slot sample base;
- sample format;
- loop control;
- pitch;
- direct level;
- pan;
- envelope defaults;
- KEY ON/OFF;
- common registers used by runtime;
- interrupt/DMA configuration if used.

## Emulator/harness tests

Where observable state is available, validate:

- SNDON initialization path;
- slot activation/completion;
- sound RAM contents/addresses;
- continuous stream buffer progression;
- no underruns during normal showcase run;
- intentional starvation increments underrun counter;
- stress mode finishes without fatal error.

Audio-quality assertions should not depend solely on a human ear if deterministic emulator state or captured output can test them.

---

# Phase 14 — Performance and latency characterization

Before declaring the runtime stable, measure and document:

```text
sound-play request -> SCSP start latency
stream refill cost
worst observed refill interval
audio update CPU time
sound-RAM upload throughput
maximum stable simultaneous SFX with music
music read bandwidth
buffer sizes and latency
SH-2 cost if any software conversion/mixing occurs
68k utilization/health indicators if applicable
```

Test at least:

- light 2D frame workload;
- heavy existing example workload suitable for integration;
- music + repeated SFX;
- near-full voice usage;
- CD/file activity while music streams.

Use the results to choose defaults. Do not optimize based only on theoretical bandwidth.

---

# Phase 15 — Public API cleanup and compatibility readiness

After `audio_showcase` is green, review public contracts.

Expected high-level vocabulary:

```text
sat_audio_init / integrated sat_init lifecycle
sat_audio_update
sat_audio_set_master_volume
sat_audio_bus_set_volume

sat_sound_t
sat_sound_load
sat_sound_play
sat_sound_unload

sat_music_t
sat_music_open
sat_music_play
sat_music_pause
sat_music_resume
sat_music_stop
sat_music_update
sat_music_close

sat_audio_stream_t
sat_audio_stream_open
sat_audio_stream_write
sat_audio_stream_available
sat_audio_stream_pause/resume
sat_audio_stream_flush
sat_audio_stream_close
```

Exact names may change after implementation. Prefer coherent LibSaturn semantics over copying another library.

## SDL2 mapping acceptance

The eventual SDL2 layer should be able to implement queued audio approximately as:

```text
SDL_OpenAudioDevice  -> sat_audio_stream_open
SDL_QueueAudio       -> sat_audio_stream_write
SDL_PauseAudioDevice -> sat_audio_stream_pause/resume
SDL_ClearQueuedAudio -> sat_audio_stream_flush
SDL_CloseAudioDevice -> sat_audio_stream_close
```

SDL code must not know about SCSP slots, sound RAM, KEY ON, or refill buffers.

## raylib mapping acceptance

The eventual raylib layer should map approximately as:

```text
LoadSound            -> sat_sound_load
PlaySound            -> sat_sound_play
UnloadSound          -> sat_sound_unload

LoadMusicStream      -> sat_music_open
PlayMusicStream      -> sat_music_play
UpdateMusicStream    -> sat_music_update
Pause/Resume/Stop    -> corresponding sat_music calls

LoadAudioStream      -> sat_audio_stream_open
UpdateAudioStream    -> sat_audio_stream_write
UnloadAudioStream    -> sat_audio_stream_close
```

If either compatibility layer must implement its own SCSP slot allocator, sound-RAM allocator, music double buffer, PCM pitch calculation, or CD refill logic, this plan is not complete.

---

# Optional Phase 16 — Advanced SCSP capabilities

These are deliberately **after** the base runtime works.

Candidates:

- SCSP DSP effects / reverb;
- send/return buses;
- envelope helpers;
- LFO helpers;
- hardware-assisted spatialization patterns;
- FM synthesis;
- MIDI-oriented facilities;
- compressed stream format if storage/bandwidth measurements justify it;
- sample-accurate scheduled starts;
- crossfade between music streams;
- ducking/side-chain style bus automation;
- richer 68k sound-driver command system.

Each must be a separate capability, not a prerequisite for basic game audio.

---

## Capacity/configuration strategy

All bounded resources must be visible and configurable.

Candidate capacities:

```text
resident sound handles
active voice metadata
SCSP slot reservations
sound-RAM allocator metadata
music objects
PCM stream objects
CPU-side stream ring buffers
sound-RAM stream buffers
command/mailbox queue if using sound CPU
```

Expose diagnostics such as:

```text
used
capacity
high-water
failed allocations
overflow count
underrun count
voice steals
```

Prefer central compile-time configuration or a coherent audio initialization config over scattered magic constants.

---

## Error-model requirements

Use `sat_result_t` consistently.

During implementation, add more specific result values only when callers can usefully distinguish them. Likely conditions include:

```text
not found
I/O failure
unsupported format/rate/channel mode
not ready
resource capacity exhausted
stream underrun state/query
invalid/stale handle
```

Underrun itself should usually be a diagnostic runtime event/counter rather than turning every update into a fatal error.

Never encode actionable failure solely in a debug string.

---

## Recommended implementation order

```text
Phase 0   audit + architecture spike
   |
Phase 1   SCSP HAL + sound-system startup
   |
Phase 2   first PCM voice
   |
Phase 3   sound-RAM allocator + transfers
   |
Phase 4   voice manager
   |
Phase 5   sat_sound_t / SFX
   |
Phase 6   buses + gain model
   |
Phase 7   generic PCM streaming
   |
Phase 8   sat_music_t
   |
Phase 9   host asset pipeline
   |
Phase 10  CD/file streaming integration
   |
Phase 11  bounded audio service/update
   |
Phase 12  audio_showcase
   |
Phase 13  automated acceptance
   |
Phase 14  perf/latency characterization
   |
Phase 15  public API cleanup + SDL/raylib readiness
   |
Phase 16  optional DSP/FM/advanced features
```

The critical dependency chain is:

```text
SCSP bring-up
   -> one correct PCM voice
      -> sound RAM + voice ownership
         -> resident SFX
            -> generic streaming
               -> music
                  -> showcase under load
                     -> compatibility layers
```

Do not start from `sat_music_play()` and backfill the hardware underneath it. Prove the lower layers first.

---

## Final architectural acceptance rule

The project has succeeded when ordinary game code can think in terms of:

```text
sound effect
music track
audio stream
volume
pan
loop
play / pause / stop
```

while LibSaturn alone owns:

```text
SMPC SNDON/SNDOFF
MC68EC000 coordination
SCSP slot registers
KEY ON/OFF
sound RAM addresses
slot reservation
voice stealing
pitch register math
PCM upload
stream buffers
refill scheduling
CD/file chunking
underrun recovery
```

The `audio_showcase` is the proof. If its gameplay/UI code needs hardware knowledge, the abstraction boundary is still wrong.
