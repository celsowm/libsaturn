# LibSaturn Audio, Music & MIDI Runtime — Execution Plan

## Purpose

Build a complete native audio foundation for LibSaturn before SDL2/raylib compatibility work depends on it.

The runtime must support four distinct game-audio workloads:

1. **resident sound effects** — short reusable PCM assets such as jump, hit, menu click, explosion;
2. **streamed music / long-form audio** — BGM and long tracks that must not consume the entire SCSP sound RAM;
3. **generic PCM streaming** — procedural audio, decoders, SDL queued audio, raylib `AudioStream`, and future source ports;
4. **sequenced music** — Standard MIDI File source material compiled offline into a Saturn-native event stream plus a trimmed instrument/sample bank.

This is not a thin wrapper over SCSP registers. LibSaturn must own the difficult Saturn-specific pieces: sound-system startup, sound RAM management, slot/voice scheduling, PCM playback, transfers, buffering, underrun handling, gain/pan policy, sequence timing, instrument/sample-bank management, CD/file streaming, and hardware limitations.

The target architecture is:

```text
                              Game / engine
                                   |
          +------------------------+------------------------+
          |                        |                        |
      sat_sound_t              sat_music_t          sat_audio_stream_t
     resident SFX             streamed PCM             generic PCM
          |                        |                        |
          +------------------------+------------------------+
                                   |
                            sat_sequence_t
                         MIDI/sequence playback
                                   |
          +------------------------+------------------------+
          |                        |                        |
       voices                   streams              sequencer/events
          |                        |                        |
          +------------------------+------------------------+
                                   |
                 sound-RAM manager / buses / scheduler
                                   |
                 transfer / refill / command transport
                                   |
                    +--------------+--------------+
                    |                             |
                 SCSP HAL                    CD / file / VFS
                    |                             |
             slots / mixer / RAM            streamed payloads
                    |
             SCSP + MC68EC000
```

SDL2, raylib, Allegro, custom engines, and future source-port layers are consumers of this runtime. They are not architectural templates for it.

---

## Hardware facts that must drive the implementation

Do not encode assumptions from memory. Verify every hardware-sensitive detail against the Sega documentation already vendored under `docs/sega_saturn_hardware`.

At minimum, implementation must account for these documented characteristics:

- the Saturn sound block includes the **MC68EC000 sound CPU, SCSP, and 4 Mbits / 512 KiB of sound RAM**;
- SCSP exposes **32 audio slots** usable for PCM/FM;
- waveform data supports documented **8-bit and 16-bit PCM formats**;
- SCSP has per-slot envelope/LFO facilities, output mixing, and DSP facilities;
- sound-system startup/reset involves the SMPC and the sound CPU;
- SMPC `SNDON`/`SNDOFF` control sound-CPU reset state;
- SCSP has interrupt and DMA facilities, but their usefulness for the chosen transport path must be measured rather than assumed;
- SCSP contains a MIDI serial interface at MIDI rate, but the retail Saturn does not provide a directly usable DIN MIDI peripheral; external MIDI hardware support therefore must not be confused with SMF/sequence playback;
- CD audio/CD data paths have different semantics and must not be conflated with PCM streamed through the runtime.

Before writing constants, cite the corresponding vendored hardware document in code comments where a value is non-obvious or easy to misinterpret.

---

## Core architectural rules

1. Work directly in `celsowm/libsaturn` on `main`.
2. Inspect current code and vendored hardware documentation before implementing each phase.
3. Run host tests after every coherent phase.
4. Build affected examples after every public API change.
5. Preserve the project's **no-runtime-heap** rule.
6. Do not use hidden `malloc`, `new`, unbounded STL containers, or static constructors.
7. Use fixed capacities, caller-owned memory, compile-time storage, or explicit arenas/pools.
8. Keep low-level SCSP APIs independently usable.
9. Do not put SDL, raylib, SMF, SF2, or desktop-codec implementation details into the SCSP HAL.
10. Do not parse OGG/MP3/FLAC/SF2 or general SMF on Saturn in the first implementation.
11. Prefer host-side conversion into bounded Saturn-native payloads.
12. Do not claim support for a sample rate, channel mode, MIDI event, controller, loop mode, transfer path, or mixer behavior until it is tested.
13. Capacity failures, voice steals, event drops, and underruns must be observable.
14. Audio timing must not depend on one update per rendered frame.
15. Commit coherent, tested phases directly to `main`.
16. Use project build/run/emulator scripts according to `AGENTS.md`.
17. The final example is an acceptance harness, not decorative sample code.

---

# Definition of done

A normal game must be able to use audio without touching SCSP registers, sound-RAM addresses, SMPC sound commands, MC68EC000 mailbox details, or transport mechanics.

The final public runtime must expose four distinct concepts:

```text
sat_sound_t
    small/medium reusable sound effect
    normally resident in sound RAM

sat_music_t
    long-form PCM-like track
    streamed/refilled without full residency

sat_audio_stream_t
    generic PCM producer/consumer stream
    used by procedural audio and compatibility layers

sat_sequence_t
    compiled musical sequence
    drives voices/instruments from a Saturn-native event stream
```

The final `examples/audio_showcase` must demonstrate simultaneously:

- deterministic sound-system bring-up;
- continuously playing/looping streamed music;
- a compiled MIDI/sequence song using an instrument bank;
- switching between streamed-music and MIDI-sequence modes;
- optional coexistence of sequence playback with an independent generic PCM stream;
- multiple overlapping SFX while music/sequence playback continues;
- mono and stereo/pan-relevant demonstrations where supported;
- per-sound/voice volume and pan;
- master/SFX/music/sequence/stream bus gain where implemented;
- looping SFX or loop-region behavior;
- pause/resume/stop/restart;
- deterministic voice exhaustion/stealing behavior;
- deterministic sound-RAM exhaustion behavior in tests;
- live stream and sequence diagnostics;
- underrun/event-lateness counters;
- no runtime heap allocation;
- stable playback while rendering a normal LibSaturn frame loop;
- build to ISO/CUE and execution through the normal emulator/harness path.

Game-facing usage should remain simple:

```c
sat_sound_t jump;
sat_music_t bgm;
sat_sequence_t battle;

SAT_TRY(sat_sound_load(&jump, "audio/jump.wav"));
SAT_TRY(sat_music_open(&bgm, "audio/showcase_theme.ogg"));
SAT_TRY(sat_sequence_open(&battle, "music/battle.mid"));

SAT_TRY(sat_music_play(bgm, SAT_AUDIO_LOOP));

while (running) {
    sat_audio_update();

    if (pad.pressed & SAT_PAD_A) {
        sat_sound_play(jump, NULL);
    }

    render_frame();
}
```

The physical ISO does not need to contain WAV/OGG/MID/SF2 files in desktop representation. Logical paths may resolve to generated Saturn-native payloads.

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
tools/
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
A. SH-2 controlled
   SH-2 writes sound RAM / SCSP slot state directly.

B. MC68EC000 driver
   SH-2 submits commands to a resident 68k sound driver.
   The sound CPU owns most slot/refill/timing work.

C. Hybrid
   SH-2 owns asset/file streaming and coarse commands;
   68k owns time-sensitive playback/refill/sequence work.
```

Create minimal spikes where needed and measure:

- implementation complexity;
- SH-2 CPU cost;
- timing stability under heavy rendering;
- command/mailbox overhead;
- refill latency/jitter;
- sequence-event jitter;
- slot ownership clarity;
- emulator support/reliability;
- assumptions likely to hold on real hardware;
- interaction with CD reads;
- debug/testability.

The bring-up may start with direct SH-2 control to reduce variables, but the long-term architecture must be selected from measurements before high-level contracts harden.

Phase gate:

- baseline recorded;
- relevant hardware constants verified;
- architecture decision backed by evidence;
- no public high-level API frozen prematurely.

---

# Phase 1 — Sound-system bring-up and SCSP HAL

Add an explicit SCSP hardware layer:

```text
src/hal/scsp.hpp
src/hal/scsp.cpp
```

plus narrowly required SMPC helpers for sound-system reset/startup.

## 1.1 Deterministic startup

Implement:

```text
system init
   -> establish known sound reset state
   -> release/initialize SCSP according to documented timing
   -> configure sound-memory/common registers
   -> initialize/clear owned sound RAM regions
   -> install/load 68k program if architecture uses one
   -> release sound CPU
   -> verify driver/SCSP readiness
   -> audio ready
```

Do not assume emulator power-on state is representative of hardware.

## 1.2 Low-level slot primitives

HAL operations must cover only verified semantics needed by the runtime:

- sample start/base;
- 8/16-bit sample format;
- loop start/end/control;
- pitch/sample-rate parameters;
- direct level/output routing;
- pan;
- envelope values needed for clean PCM playback;
- KEY ON / KEY OFF;
- slot status/completion;
- common mixer/global controls used by the runtime.

Keep register bit layout inside HAL.

## 1.3 MIDI hardware registers

Implement low-level MIDI-register encoding/access only if it naturally belongs in the HAL work being touched, but keep it clearly separate from `sat_sequence_t`.

Possible low-level future-facing names:

```text
sat_scsp_midi_status
sat_scsp_midi_read
sat_scsp_midi_write
```

These APIs are **not required** by the initial showcase because external MIDI requires non-standard hardware/interface support.

Phase gate:

- host tests cover pure register encoding logic;
- deterministic SCSP init sequence exists;
- no magic register values leak above HAL;
- existing non-audio examples still build.

---

# Phase 2 — First PCM: prove one voice end to end

Before managers, play one known repository-generated waveform.

Acceptance fixture:

```text
440 Hz tone
mono
known sample count
known amplitude
known source sample rate
```

Prove:

- 8-bit PCM where supported;
- 16-bit PCM;
- correct pitch at multiple source rates;
- KEY ON starts cleanly;
- KEY OFF stops cleanly;
- one-shot completion;
- looping;
- reset/reinit does not leave stale playback.

If emulator capture or deterministic state probes can verify frequency/duration automatically, add them to the harness.

---

# Phase 3 — Sound RAM allocator and transfer layer

Treat SCSP sound RAM as a first-class bounded resource.

## 3.1 Deterministic allocator

Internal API concept:

```text
sound_ram_init
sound_ram_alloc
sound_ram_free
sound_ram_stats
```

Requirements:

- no general heap;
- deterministic failure;
- documented alignment;
- explicit reserved areas for 68k code, mailboxes, stream buffers, sequence bank samples, and runtime metadata;
- coalescing/fragmentation policy if frees exist;
- diagnostics: `used`, `free`, `largest_free`, `high_water`, failed allocations;
- no overlap with sound-CPU program/data.

## 3.2 Transfer abstraction

Separate source from transport:

```text
CPU / file / generated bytes
            |
      transfer layer
            |
         sound RAM
```

Benchmark realistic paths. Do not assume the SCSP internal DMA is a generic main-CPU-to-sound-RAM DMA engine.

Expose upload telemetry.

Phase gate:

- allocator stress/exhaustion/reuse tests;
- repeated upload/play/free cycles;
- measured transfer behavior documented.

---

# Phase 4 — Voice manager

A **voice** is a currently playing hardware instance. A **sound/instrument sample** is reusable data.

Conceptual internal handle:

```c
typedef struct sat_voice {
    uint8_t slot;
    uint8_t generation;
} sat_voice_t;
```

Responsibilities:

- allocate/release SCSP slots;
- track one-shot completion;
- looping;
- volume/pan;
- pitch/playback-rate;
- priority;
- owner category: SFX / MUSIC / STREAM / SEQUENCE;
- protected/reserved voices where required;
- generation-safe handles;
- deterministic exhaustion.

## 4.1 Voice stealing

Baseline policy:

1. free eligible voice;
2. otherwise lowest-priority stealable voice;
3. tie -> oldest start;
4. protected stream/sequence/music voices are not stolen unless explicitly permitted.

Sequence notes must participate in this policy deliberately; a dense MIDI arrangement must not randomly destroy critical SFX or stream channels.

Diagnostics:

```text
voices active/capacity
voice steals by owner
failed play/note requests
per-bus active count
peak simultaneous voices
```

---

# Phase 5 — High-level resident sound API

Add game-facing audio types without hardware addresses.

Conceptual `sat_sound_t` is a generation-safe handle.

Target operations:

```text
sat_sound_load / create
sat_sound_unload
sat_sound_play
sat_sound_stop_all_instances
sat_sound_info
```

Play params should support at least:

```text
volume
pan
priority
loop/flags
optional pitch ratio
```

Required semantics:

- one sound may have multiple active instances;
- unload-while-playing behavior documented;
- stale handles fail safely;
- source sample rate preserved by pitch calculation;
- loop metadata explicit;
- SFX/master bus interaction deterministic.

---

# Phase 6 — Audio buses and gain model

Provide a small bounded hierarchy:

```text
MASTER
  +-- SFX
  +-- MUSIC
  +-- SEQUENCE
  +-- STREAM / AUX
```

Target operations:

```text
sat_audio_set_master_volume
sat_audio_bus_set_volume
sat_audio_bus_get_volume
```

Specify:

- gain representation;
- clamping;
- whether changes affect already-playing voices immediately;
- pan law;
- source x voice x bus x master composition;
- hardware vs software gain application.

Avoid per-sample SH-2 mixing when the SCSP can do the job.

---

# Phase 7 — Generic PCM streaming core

### Implementation status — 2026-09-17

The caller-owned CPU-side ring-buffer core is implemented in LibSaturn with
fixed stream slots, generation-safe handles, mono PCM S8/S16 validation,
deterministic rejected writes, pause/resume/flush, and diagnostics. Host tests
cover wraparound and public lifecycle behavior.

The SCSP refill/consumption scheduler is deliberately still open. This phase
must not be called complete until the ring is connected to measured SCSP
playback and the long-run, starvation, and pause/resume gates below pass.

This is the primitive for long PCM music, procedural audio, SDL queued audio, and raylib streams.

Conceptual configuration:

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

## 7.1 Bounded buffering

```text
producer
   |
CPU-side fixed ring/staging
   |
refill scheduler
   |
SCSP sound-RAM playback buffers
   |
SCSP voice(s)
```

If 68k/hybrid architecture owns refill, public semantics remain unchanged.

## 7.2 Underruns

Track:

```text
underrun_count
rejected/overrun_write_count
queued frames
minimum fill
maximum fill
```

Define deterministic underrun behavior.

## 7.3 Stereo

Prove synchronization experimentally before advertising stereo stream support.

Phase gate:

- multi-minute stream without drift/crash;
- intentional starvation increments underruns predictably;
- pause/resume stable;
- pure host ring-buffer tests;
- stereo long-run test if enabled.

---

# Phase 8 — Streamed music runtime

Build `sat_music_t` on `sat_audio_stream_t`, not as a separate engine.

Target operations:

```text
sat_music_open
sat_music_play
sat_music_pause
sat_music_resume
sat_music_stop
sat_music_seek          optional initially
sat_music_set_loop
sat_music_update
sat_music_close
sat_music_info
sat_music_stats
```

Support full-track looping first. Add intro+loop and sample-accurate loop points only when the physical format/backend can deliver them without gaps.

Seek may initially return `SAT_ERR_UNSUPPORTED`.

Phase gate:

- looping music under normal rendering;
- SFX overlap cleanly;
- pause/resume/stop/restart stable;
- multi-minute stress run;
- no resource leaks on repeated open/close.

---

# Phase 9 — First-class host audio toolchain

The toolchain is an official deliverable, not build-script glue.

Create and document:

```text
tools/convert_audio.py
tools/inspect_audio.py
tools/pack_audio_assets.py
tools/convert_midi.py
tools/inspect_midi.py
tools/build_midi_bank.py
tools/generate_audio_showcase_assets.py
```

Use shared Python modules internally if needed; do not duplicate codec/resample/metadata logic across scripts.

## 9.1 `convert_audio.py`

Purpose:

```text
WAV / OGG / FLAC / supported desktop source
                    |
              host conversion
                    |
          Saturn-native payload
```

Responsibilities:

- decode source through a documented host dependency or standard-library path where possible;
- normalize only when explicitly requested;
- resample;
- mono/stereo conversion;
- 8/16-bit PCM conversion;
- clipping/dither policy explicit;
- loop metadata import/override;
- deterministic payload writer;
- `--kind sound|music|stream` presets;
- machine-readable metadata output.

Example UX:

```bash
python tools/convert_audio.py audio/jump.wav \
  --kind sound \
  --output build/audio/jump.satpcm

python tools/convert_audio.py audio/theme.ogg \
  --kind music \
  --output build/audio/theme.satstream
```

Do not hard-depend on one external executable in every Makefile. If ffmpeg or another decoder is used, isolate and validate the dependency in the tool.

## 9.2 `inspect_audio.py`

Must report useful source-port/runtime budgeting information:

```text
source format
duration
source rate/channels/bit depth
converted rate/channels/format
payload bytes
estimated stream bandwidth
recommended/selected buffer size
resident sound-RAM cost
loop points
peak amplitude / clipping warning
```

Support inspection of both desktop inputs and generated Saturn payloads.

## 9.3 `pack_audio_assets.py`

Purpose:

- build asset/VFS metadata;
- preserve logical source paths;
- associate logical `audio/jump.wav` with generated payload;
- associate logical `music/theme.ogg` with stream payload;
- associate logical `music/battle.mid` with `.satseq` + bank;
- emit deterministic manifests/registry data;
- avoid embedding per-example conversion hacks in Makefiles.

## 9.4 Determinism

Tools must support reproducible CI outputs. Repository fixtures should have stable metadata/size/hash expectations where practical.

---

# Phase 10 — MIDI/sequence compilation and runtime

MIDI support means **sequenced music**, not pretending the retail Saturn has a normal MIDI DIN port.

Desktop authoring input:

```text
song.mid
   +
SF2 / sample bank / explicit instrument manifest
   |
   +--> tools/convert_midi.py
   +--> tools/build_midi_bank.py
   |
   v
song.satseq
song.satbank
```

The Saturn runtime should consume compact prevalidated formats. It should not need a general SMF parser or SF2 parser initially.

## 10.1 SMF source support

Initial tool support should target:

```text
SMF format 0
SMF format 1
PPQN timing
multiple tracks
running status
standard tempo meta events
```

Reject unsupported/corrupt files with precise diagnostics.

## 10.2 Event subset for first runtime

Required:

```text
Note On
Note Off
Velocity
Program Change
Control Change: volume
Control Change: pan
Control Change: expression
Control Change: sustain pedal
Pitch Bend
Tempo changes
Channel 10 percussion convention
```

Later optional:

```text
aftertouch
channel pressure
RPN/NRPN
selected SysEx semantics
richer controller automation
```

Unknown metadata should be discarded or preserved according to an explicit tool policy, never silently reinterpreted.

## 10.3 `tools/convert_midi.py`

Responsibilities:

- parse SMF;
- merge/normalize track timing without destroying channel semantics;
- canonicalize running status;
- build tempo map;
- validate event ordering;
- resolve instrument/program usage;
- calculate polyphony over time;
- generate compact `.satseq` event data;
- emit diagnostics for events not supported by runtime;
- preserve useful track/channel names in debug metadata, not necessarily runtime payload;
- optionally precompute event deadlines/lookahead blocks to reduce runtime work.

The output format must be versioned and endian-explicit.

## 10.4 `inspect_midi.py`

Report:

```text
SMF format
tracks
PPQN
duration
tempo changes
channels used
programs used
percussion usage
controllers used
pitch bend usage
max simultaneous notes
max notes by channel
estimated SCSP slot pressure
unsupported events
bank/sample estimate
```

Example:

```text
File: boss.mid
Format: SMF 1
Tracks: 9
Duration: 03:21.400
PPQN: 480
Tempo changes: 3
Max simultaneous notes: 18
Peak estimated SCSP voices: 18 / 32
Programs: Piano, Bass, Strings, Lead
Channel 10: drums
Pitch bend: yes
Sustain: yes
```

## 10.5 `build_midi_bank.py`

Build only the instruments/sample regions actually required by one song or a declared song set.

Inputs may include:

```text
SF2
explicit WAV sample manifest
future native instrument description
```

Responsibilities:

- map MIDI program/percussion keys to instrument regions;
- trim unused programs and samples;
- resample/convert samples to SCSP-friendly format;
- preserve sample loop points;
- generate key/velocity region metadata;
- generate root-key/tuning data;
- deduplicate identical samples;
- estimate sound-RAM residency;
- support policies for always-resident vs load-on-song-start banks;
- deterministic output.

Do **not** attempt to load a full General MIDI soundfont into sound RAM by default.

## 10.6 `sat_sequence_t`

Conceptual public API:

```text
sat_sequence_open
sat_sequence_play
sat_sequence_pause
sat_sequence_resume
sat_sequence_stop
sat_sequence_seek          optional initially
sat_sequence_set_loop
sat_sequence_set_tempo_scale
sat_sequence_update
sat_sequence_close
sat_sequence_info
sat_sequence_stats
```

A sequence object owns compact event state and a logical instrument bank, not hardware slots directly.

## 10.7 Sequence scheduling

Sequence timing must be tied to a monotonic/audio clock, not rendered frames.

Bad:

```text
one MIDI tick batch per game frame
```

Required concept:

```text
canonical sequence time
        |
        v
lookahead scheduler
        |
voice/note commands with deadlines
        |
SCSP/68k execution
```

If the chosen 68k driver architecture can accept timestamped/lookahead events, prefer that for low jitter. If scheduling remains on SH-2, `sat_audio_update()` must process bounded lookahead and tolerate variable frame duration without musically audible drift.

Track:

```text
sequence current time/tick
late event count
maximum event lateness
queued lookahead events
notes active
notes stolen
loop count
```

## 10.8 Instrument/sample voice semantics

Map:

```text
Note On       -> instrument region lookup -> voice start
Note Off      -> release/key-off policy
velocity      -> level
audio channel volume/expression -> gain
pan           -> SCSP pan
note number   -> root-key-relative pitch
pitch bend    -> pitch update
sustain       -> deferred release
program       -> instrument selection
channel 10    -> percussion-key mapping
```

Envelope policy may come from the bank/instrument definition and use SCSP envelope facilities where appropriate.

## 10.9 Sequence voice pressure

MIDI polyphony competes for the same finite SCSP slots as SFX/streaming.

Define explicit policy:

- reserve minimum critical stream voices;
- sequence notes have configurable priority class;
- release tails may be shortened/stolen before critical attacks where musically preferable;
- drum hits may use different stealing priority from sustained melodic notes;
- expose sequence note steals separately from SFX steals.

Phase gate:

- deterministic SMF fixture compiles;
- scale/arpeggio sequence plays correct pitches;
- Program Change selects correct instrument;
- sustain behaves correctly;
- pitch bend audible/state-verifiable;
- tempo change stays in time;
- dense chord stress exercises voice policy deterministically;
- sequence timing does not slow when render frames are intentionally delayed.

---

# Phase 11 — File/CD/VFS integration

Music and sequences must coexist with game I/O.

Requirements:

- partial/refill-sized reads;
- bounded staging;
- EOF/loop restart;
- read errors propagated to state/stats;
- no whole-file allocation for streamed PCM;
- sequence payload may be small enough for whole-file loading only when bounded policy explicitly permits it;
- instrument-bank loading has explicit sound-RAM budget;
- no assumption that CD reads complete instantly;
- avoid long blocking refill inside latency-sensitive paths.

Stress scenarios:

```text
streamed music + SFX burst
MIDI sequence + SFX burst
MIDI sequence + generic PCM stream
streamed music + unrelated asset read
sequence bank load/unload cycles
music loop at EOF + SFX burst
intentional delayed refill
```

---

# Phase 12 — Bounded update/service model

Define one obvious game rule:

```c
sat_audio_update();
```

or prove that no global call is needed.

If retained, it performs bounded service work:

- reap completed voices;
- process stream/music refills;
- advance sequence lookahead;
- enqueue due/near-due sequence events;
- apply deferred commands;
- update diagnostics;
- service sound-CPU mailbox if used.

Requirements:

- bounded maximum work per call;
- no unbounded decode/parse;
- safe once per game frame;
- behavior under slow frames documented;
- audio clock independent of frame count;
- long frame cannot make musical time itself slow down;
- excessive lateness is counted and visible.

---

# Phase 13 — `examples/audio_showcase`

Create a first-class interactive acceptance example:

```text
examples/audio_showcase/
    main.c
    Makefile.inc
    assets/
        source/ or declarations
        audio/
        midi/
```

The example must be a small **audio test console**, not a blank screen playing a tone.

## 13.1 Host-generated fixtures

Use repository-owned deterministic fixtures generated by:

```text
tools/generate_audio_showcase_assets.py
```

Generate at least:

- short sine/pluck SFX;
- short noise/percussion SFX;
- sweep SFX;
- pan-identifiable/stereo fixture;
- looping ambience SFX;
- multi-minute synthetic streamed BGM;
- generic procedural PCM stream pattern;
- a deterministic SMF song with drums, bass, chords and melody;
- a minimal sample/instrument bank source suitable for the MIDI song.

The MIDI fixture must exercise:

```text
Note On/Off
velocity
Program Change
pan
volume/expression
sustain
pitch bend
tempo change
channel 10 percussion
```

No copyrighted assets.

## 13.2 Visual HUD

Example HUD:

```text
LIBSATURN AUDIO SHOWCASE

MODE    MIDI SEQUENCE
SONG    showcase.mid
STATE   PLAYING / LOOP
TIME    00:42.318
TEMPO   128 BPM
TICK    038420

SFX     sweep
PAN     +00
VOL     100%

VOICES  16 / 32
  SFX    3
  SEQ   11
  STRM   2
SND RAM 244K / 512K
STREAM  71% full
UNDERRUNS 0
STEALS     3
SEQ LATE   0

A      play selected SFX
B      burst / overlap stress
C      toggle selected SFX loop
X/Y    select SFX
L/R    pan
UP/DN  volume
Z      toggle generic PCM stream
START  pause/resume current music source
```

Provide an intuitive button combination to switch between:

```text
STREAMED BGM
MIDI SEQUENCE
```

and another for stress mode / restart / loop toggle as control space permits.

## 13.3 MIDI display

When sequence mode is active, also show useful state:

```text
tracks/channels
active notes
current programs
sequence loop count
late events
maximum event lateness
sequence note steals
bank RAM usage
```

A small piano/key or channel activity visualization is optional but useful if simple.

## 13.4 Stress mode

Deterministic automatic stress mode:

```text
- current music source continues;
- periodic SFX bursts;
- pan alternates;
- volume changes in fixed sequence;
- generic PCM stream may run simultaneously;
- optional unrelated file reads;
- MIDI mode periodically hits dense chords/drums;
- all counters remain visible.
```

Harness must be able to enter stress mode through deterministic input and run it for a fixed duration.

## 13.5 Acceptance telemetry

Expose at least:

```text
active voices / capacity / peak
voice steals by owner
failed play/note count
sound RAM used/capacity/high-water/largest-free
stream buffered frames/percentage
stream underrun/rejected-write counts
music state/loop count/bytes read
sequence state/time/tick/loop count
sequence active notes
sequence late-event count/max lateness
sequence note steals
bank bytes/resident instruments
last audio error
```

If a 68k driver is used, expose mailbox/driver health counters.

---

# Phase 14 — Automated testing

## Host tests

Keep pure logic host-testable:

- pitch/sample-rate conversion;
- gain/pan math;
- sound-RAM allocator and fragmentation;
- voice allocation/stealing;
- generation handles;
- ring-buffer wraparound;
- underrun/overrun accounting;
- music state machine;
- sequence state machine;
- SMF parser/converter fixtures;
- running-status parsing;
- tempo-map conversion;
- event ordering;
- controller canonicalization;
- polyphony calculation;
- instrument-region lookup;
- bank sample deduplication;
- MIDI note -> SCSP pitch math;
- pitch bend;
- sustain/deferred release;
- bounded mailbox logic if used;
- generated asset determinism.

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
- interrupt/DMA configuration if used;
- MIDI hardware registers if low-level support is added.

## Emulator/harness tests

Validate where observable:

- sound startup;
- slot activation/completion;
- sound RAM contents/addresses;
- stream progression;
- no normal-run underruns;
- intentional starvation increments underrun counter;
- MIDI sequence progression;
- tempo-change timing;
- render slowdown does not proportionally slow the sequence;
- stress mode finishes without fatal error.

Prefer deterministic emulator state/captured output checks over human-ear-only assertions.

---

# Phase 15 — Performance, latency, and jitter characterization

Measure and document:

```text
sound-play request -> SCSP start latency
stream refill cost
worst refill interval
audio update CPU time
sound-RAM upload throughput
maximum stable simultaneous SFX with streaming
maximum stable sequence polyphony with SFX
music read bandwidth
buffer sizes and end-to-end latency
sequence event scheduling jitter
maximum observed sequence lateness
SH-2 cost of conversion/mixing/scheduling
68k utilization/health indicators if applicable
bank load time
```

Test:

- light 2D workload;
- heavy existing example workload;
- streamed music + repeated SFX;
- MIDI sequence + repeated SFX;
- MIDI + generic PCM stream;
- near-full voice usage;
- CD/file activity during playback.

Choose defaults from measurements, not theoretical bandwidth.

---

# Phase 16 — Public API cleanup and compatibility readiness

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
sat_music_pause/resume/stop
sat_music_update
sat_music_close

sat_audio_stream_t
sat_audio_stream_open
sat_audio_stream_write
sat_audio_stream_available/buffered
sat_audio_stream_pause/resume
sat_audio_stream_flush
sat_audio_stream_close

sat_sequence_t
sat_sequence_open
sat_sequence_play
sat_sequence_pause/resume/stop
sat_sequence_set_loop
sat_sequence_set_tempo_scale
sat_sequence_update
sat_sequence_close
```

Exact names may change after implementation. Prefer coherent LibSaturn semantics over copying another library.

## SDL2 acceptance

Queued audio should map approximately as:

```text
SDL_OpenAudioDevice   -> sat_audio_stream_open
SDL_QueueAudio        -> sat_audio_stream_write
SDL_PauseAudioDevice  -> sat_audio_stream_pause/resume
SDL_ClearQueuedAudio  -> sat_audio_stream_flush
SDL_CloseAudioDevice  -> sat_audio_stream_close
```

SDL code must not know SCSP slots, sound RAM, KEY ON, or refill buffers.

## raylib acceptance

```text
LoadSound             -> sat_sound_load
PlaySound             -> sat_sound_play
UnloadSound           -> sat_sound_unload

LoadMusicStream       -> sat_music_open
PlayMusicStream       -> sat_music_play
UpdateMusicStream     -> sat_music_update
Pause/Resume/Stop     -> corresponding sat_music calls

LoadAudioStream       -> sat_audio_stream_open
UpdateAudioStream     -> sat_audio_stream_write
UnloadAudioStream     -> sat_audio_stream_close
```

Raylib does not require a MIDI API mapping, but a raylib/source-port game that uses its own MIDI sequencer should be able to feed the LibSaturn sound/voice layer without learning SCSP details.

## Source-port MIDI acceptance

A port using MIDI assets should need no SCSP-specific rewrite beyond replacing its sequencer backend or using `sat_sequence_t`.

The compatibility/source-port layer must not implement:

- SCSP slot allocation;
- sound-RAM allocation;
- sample bank trimming;
- MIDI note-to-pitch register math;
- sustain/voice lifecycle hardware details;
- stream double buffering;
- CD refill logic.

---

# Optional Phase 17 — Advanced SCSP capabilities

Only after the base runtime, tools, MIDI sequence path, and showcase are green:

- SCSP DSP effects / reverb;
- send/return buses;
- richer envelope helpers;
- LFO helpers;
- hardware-assisted spatialization patterns;
- FM instrument synthesis;
- MIDI banks using native SCSP FM instruments;
- external MIDI serial I/O for users with suitable hardware/interface;
- compressed stream formats if measurements justify them;
- sample-accurate scheduled starts;
- music/sequence crossfade;
- ducking/side-chain style bus automation;
- richer 68k command system;
- RPN/NRPN/aftertouch/SysEx subsets.

FM MIDI instruments are especially interesting for Saturn, but they must be a capability added after sample-based sequencing is proven.

---

## Tooling deliverables summary

The audio work is not complete without these host tools or equivalent consolidated commands:

```text
convert_audio.py
    desktop audio -> Saturn PCM/stream payload

inspect_audio.py
    inspect source and Saturn payload budgets/metadata

pack_audio_assets.py
    logical-path registry / VFS metadata

convert_midi.py
    SMF 0/1 -> compact versioned .satseq

inspect_midi.py
    timing, instruments, controllers, polyphony, slot pressure

build_midi_bank.py
    used instruments/samples -> compact .satbank

generate_audio_showcase_assets.py
    deterministic copyrighted-free acceptance assets
```

The Makefile/build system should invoke these through stable project-level rules rather than embedding conversion logic separately into each example.

---

## Capacity/configuration strategy

All bounded resources must be visible/configurable:

```text
resident sound handles
active voice metadata
SCSP slot reservations
sound-RAM allocator metadata
music objects
PCM stream objects
sequence objects
sequence event lookahead queue
instrument-bank handles
CPU-side stream rings
sound-RAM stream buffers
68k command/mailbox queue
```

Expose diagnostics:

```text
used
capacity
high-water
failed allocations
overflow count
underrun count
voice steals
late sequence events
```

Prefer central compile-time configuration or coherent audio init config over scattered magic constants.

---

## Error-model requirements

Use `sat_result_t` consistently.

Add specific errors only when callers can act on them. Likely conditions:

```text
not found
I/O failure
unsupported audio format/rate/channel mode
unsupported MIDI event/source feature
invalid/corrupt sequence or bank
not ready
resource capacity exhausted
bank does not fit
invalid/stale handle
```

Underrun/event lateness should generally be runtime diagnostics rather than making every update a fatal error.

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
Phase 6   buses + gain
   |
Phase 7   generic PCM streaming
   |
Phase 8   sat_music_t / streamed music
   |
Phase 9   official host audio toolchain
   |
Phase 10  MIDI compiler + bank builder + sat_sequence_t
   |
Phase 11  CD/file/VFS integration
   |
Phase 12  bounded audio service/update
   |
Phase 13  audio_showcase: PCM + stream + MIDI
   |
Phase 14  automated acceptance
   |
Phase 15  performance / latency / MIDI jitter
   |
Phase 16  public API cleanup + compatibility readiness
   |
Phase 17  optional DSP / FM / external MIDI / advanced features
```

Critical dependency chain:

```text
SCSP bring-up
   -> one correct PCM voice
      -> sound RAM + voice ownership
         -> resident SFX
            -> generic PCM streaming
               -> streamed music
                  -> deterministic asset tools
                     -> MIDI sequence + compact bank
                        -> showcase under load
                           -> compatibility/source-port layers
```

Do not begin from `sat_music_play()` or `sat_sequence_play()` and backfill hardware underneath them. Prove lower layers first.

---

## Final architectural acceptance rule

The project succeeds when ordinary game code can think in terms of:

```text
sound effect
streamed music
PCM stream
MIDI/sequence song
instrument bank
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
sequence clock/lookahead
MIDI event -> voice semantics
instrument-region selection
bank residency
CD/file chunking
underrun recovery
```

`examples/audio_showcase` is the proof. If its gameplay/UI code needs SCSP register knowledge, manual sound-RAM addresses, SMF parsing, SF2 parsing, or custom refill mechanics, the abstraction boundary is still wrong.
