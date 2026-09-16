# High-Level Runtime ↔ Audio/MIDI Integration Contract

## Purpose

This document connects `HIGH_LEVEL_RUNTIME_API_PLAN.md` with `AUDIO_MUSIC_RUNTIME_PLAN.md` without duplicating the SCSP/audio execution plan inside the general runtime document.

`AUDIO_MUSIC_RUNTIME_PLAN.md` is the **authoritative execution subplan** for the high-level runtime's audio capability.

If the two documents appear to conflict on audio behavior, resource ownership, tooling, sequencing, or SCSP architecture, the dedicated audio plan wins for those topics and the high-level runtime plan should be updated to match after the audio contract stabilizes.

---

## High-level runtime audio readiness

The high-level runtime must not be considered audio-ready until the dedicated audio plan has delivered and tested these native concepts:

```text
sat_sound_t
    resident/reusable sound effects

sat_music_t
    long-form streamed PCM-style music

sat_audio_stream_t
    generic bounded PCM producer/consumer stream

sat_sequence_t
    compiled MIDI/sequence playback using Saturn-native event + bank payloads
```

The runtime must own all Saturn-specific complexity below these concepts:

```text
SMPC sound startup
MC68EC000 coordination
SCSP register programming
sound-RAM allocation
slot/voice allocation
voice stealing
sample pitch math
PCM upload
stream buffering/refill
sequence timing/lookahead
MIDI event -> voice semantics
instrument/sample-bank residency
CD/file chunking
underrun and lateness recovery/diagnostics
```

Compatibility layers and game code must not reimplement those responsibilities.

---

## Asset/VFS contract

The logical asset layer defined by the high-level runtime must support source-oriented paths even when the Saturn ISO stores converted payloads.

Examples:

```text
audio/jump.wav
    -> Saturn resident PCM payload

music/theme.ogg
    -> Saturn stream payload

music/battle.mid
    -> battle.satseq + battle.satbank
```

Game-facing APIs may therefore use logical source paths such as:

```c
sat_sound_load(&jump, "audio/jump.wav");
sat_music_open(&theme, "music/theme.ogg");
sat_sequence_open(&battle, "music/battle.mid");
```

The runtime/VFS decides which generated physical payloads satisfy those logical requests.

---

## Required host tooling

The high-level asset pipeline is not complete for audio until the dedicated audio plan's tooling (or consolidated equivalents) exists:

```text
tools/convert_audio.py
tools/inspect_audio.py
tools/pack_audio_assets.py
tools/convert_midi.py
tools/inspect_midi.py
tools/build_midi_bank.py
tools/generate_audio_showcase_assets.py
```

Build integration must invoke stable project-level tooling rules. Individual examples or compatibility layers must not carry private ffmpeg/Python conversion hacks.

---

## Acceptance dependency

`examples/audio_showcase` is the authoritative native acceptance test for the audio subsystem.

Before the high-level runtime can claim the audio portion of its native acceptance gates, `audio_showcase` must prove at least:

- streamed music;
- overlapping resident SFX;
- generic PCM streaming;
- compiled MIDI/sequence playback;
- instrument-bank loading;
- volume/pan/loop behavior;
- deterministic voice pressure/stealing;
- sound-RAM diagnostics;
- stream underrun diagnostics;
- sequence lateness/jitter diagnostics;
- stable playback under a normal render loop;
- no runtime heap allocation.

`runtime_2d` may then consume these audio APIs as a normal game would; it must not become a second SCSP test harness.

---

## SDL2 readiness

For the initial SDL2 compatibility layer, queued audio should remain a thin mapping:

```text
SDL_OpenAudioDevice   -> sat_audio_stream_open
SDL_QueueAudio        -> sat_audio_stream_write
SDL_PauseAudioDevice  -> sat_audio_stream_pause/resume
SDL_ClearQueuedAudio  -> sat_audio_stream_flush
SDL_CloseAudioDevice  -> sat_audio_stream_close
```

SDL code must not know about SCSP slots, sound RAM, refill buffers, or the sound CPU.

MIDI is not required by SDL2 itself, but SDL-based game ports that use MIDI assets can use `sat_sequence_t` or replace only their sequencer-facing layer.

---

## raylib readiness

The initial raylib compatibility layer should be able to map:

```text
LoadSound           -> sat_sound_load
PlaySound           -> sat_sound_play
UnloadSound         -> sat_sound_unload

LoadMusicStream     -> sat_music_open
PlayMusicStream     -> sat_music_play
UpdateMusicStream   -> sat_music_update / sat_audio_update
Pause/Resume/Stop   -> corresponding music calls

LoadAudioStream     -> sat_audio_stream_open
UpdateAudioStream   -> sat_audio_stream_write
UnloadAudioStream   -> sat_audio_stream_close
```

A raylib-based port with MIDI assets should not need SCSP-specific code; it may use `sat_sequence_t` directly behind its own game-level music abstraction.

---

## Architectural gate

If SDL, raylib, Allegro, or another source-port adapter must do any of the following, the high-level audio boundary is incomplete and work must return to `AUDIO_MUSIC_RUNTIME_PLAN.md`:

```text
allocate SCSP slots
allocate sound RAM
calculate SCSP pitch registers
manually KEY ON/OFF notes
implement PCM double/ring buffering
schedule MIDI events from render frames
parse SF2/SMF on Saturn to compensate for missing tooling
trim instrument banks itself
perform CD refill scheduling
recover audio underruns directly
```

The desired adapter code translates semantics. LibSaturn owns the Saturn.
