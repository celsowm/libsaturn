# SCSP audio streaming and harness guide

This document is the practical bridge between LibSaturn's high-level audio runtime,
the SCSP hardware manuals mirrored under `docs/sega_saturn_hardware/`, and the
Ymir-based harness. It is intentionally implementation-oriented: use the original
manual pages for register semantics and this guide for LibSaturn invariants and
regression checks.

## Source-of-truth manual pages

Read these before changing the SCSP HAL or streaming scheduler:

- [SCSP overview / native sampling model](sega_saturn_hardware/hard/scsp/hon/p02_10.md)
- [Loop control, SA/LSA/LEA and PCM8B](sega_saturn_hardware/hard/scsp/hon/p04_21.md)
- [PITCH: OCT and FNS](sega_saturn_hardware/hard/scsp/hon/p04_25.md)
- [Digital mixer: DISDL, DIPAN and clipping](sega_saturn_hardware/hard/scsp/hon/p04_27.md)
- [Slot status: MSLC and CA](sega_saturn_hardware/hard/scsp/hon/p04_28.md)

Do not infer SCSP behavior from a desktop audio API. The chip has its own sample
addressing, pitch, mixer and key-on model.

## Hardware facts that matter to LibSaturn

- The SCSP sound-generator resampling clock is 44.1 kHz.
- PCM waveform data is signed two's-complement, either 16-bit or 8-bit.
- For 16-bit PCM, SA must be even.
- LSA and LEA are sample offsets relative to SA, not byte offsets.
- `OCT=0, FNS=0` plays a source at its sampling rate. Therefore a 22.05 kHz
  source played at real speed is `OCT=-1 (0xF), FNS=0`.
- `DISDL=7` is 0 dB direct send. It is safe only when the mix reaching a DAC
  side cannot overflow; lowering MVOL after overflow does not repair clipping.
- The slot-status CA field reports the current sample position in 4096-sample
  units. This is especially useful because LibSaturn's streaming half-buffer is
  also 4096 samples.

## LibSaturn streaming contract

The current stream runtime reserves SCSP slots 28-31 and the top of sound RAM.
For S16 streaming:

- one hardware half = 4096 PCM samples = 8192 bytes;
- one continuous hardware loop = two halves = 8192 samples;
- the hardware slot remains keyed on while software refills the inactive half;
- the CPU-side ring must be primed with both halves before key-on;
- steady-state code must never overwrite the half the SCSP is currently reading;
- `underrun_count` should remain zero during normal music playback.

Stereo music is implemented as two synchronized mono streams. The interleaved
source is deinterleaved without changing the two bytes of each S16 sample, then
the left and right streams are hard-panned to separate DAC sides.

For the first stereo music instance, the expected SCSP slots are normally 28 and
29. At 22.05 kHz S16, useful harness expectations are:

| Field | Expected |
| --- | --- |
| PCM8B | 0 (16-bit signed PCM) |
| LPCTL | normal loop |
| LSA | 0 |
| LEA | 8191 |
| OCT | 0xF (-1 octave) |
| FNS | 0 |
| TL | 0 |
| DISDL | 7 (0 dB) |
| left DIPAN | 0x1F |
| right DIPAN | 0x0F |

Do not turn this table into a generic requirement for every sound effect. It is
the expected state for the current 22.05 kHz stereo streaming path.

## CD jukebox path

CD Block rules and timing limits: `docs/sega_saturn_hardware/hard/cdblock/index.md`.

`examples/cd_streaming_jukebox` currently uses this pipeline:

1. vendored 44.1 kHz S16 big-endian stereo source;
2. deterministic 2:1 downsample while preserving S16 big-endian byte order;
3. CD-backed asset read;
4. `sat_music_*` software ring;
5. stereo deinterleave into two mono rings;
6. SCSP double-buffer upload;
7. continuous normal-loop playback on two hard-panned slots.

The output of step 2 is 22.05 kHz. A wrong endianness conversion here normally
sounds like harsh full-spectrum noise; a wrong PITCH value changes pitch/tempo;
rewriting the active SCSP half produces periodic bursts, metallic corruption or
apparently "exploded" music.

For CPU-heavy scenes, `sat_music` can also read 1024-frame IMA ADPCM blocks
from the disc. The build converts the source and emits the exact frame count;
the runtime decodes each block into planar S16BE before writing the same
software rings used by PCM tracks. Each block begins with an exact predictor
and step index per channel, so seeking and looping need no previous block.
The SCSP slot format and refill rules above are unchanged.

### Disc-scheduled music

`docs/sega_saturn_hardware/hard/cdblock/index.md` collects what the Overview Manual and Sega's CD
communication I/F guide say about the CD Block (registers, flags, the 30-commands-per-6.7-ms limit, the
ban on continuous command polling, the 200-sector buffer, 150 sectors/s at 2x, the data-transfer
procedure, SCU-DMA direct mode only) and marks every figure that is only an emulator measurement. In
short: a poll costs commands, not sectors, so the scheduler moves whole segments per poll; the numbers
below are Mednafen/Ymir, not hardware; and the manual's 500 ms (2x) / 800 ms (1x) average seek is longer
than the music reserve, which no emulator run shows (see "Seek time on real hardware" there).

By default `sat_music_update` reads the next 64 KiB of the track with a blocking CD read, so the
frame waits for the drive (about 200 ms at 2x speed; measured on Mednafen the worst `sat_music_update`
was 359,709 us and the worst frame 381 ms). The rings hold 1.5 s, so the audio does not underrun,
but a game stalls every few seconds. Bind the disc scheduler once and the same API streams in the
background:

```c
static sat_disc_stream_t disc;                      /* ~32 KiB: put it in .wram_l */
sat_disc_stream_init(&disc, &cd_block);
sat_music_use_disc_stream(&disc);                   /* before sat_music_open */
```

A track registered with `sat_cdfs_register_source_manifest` and opened with `sat_music_open` then
reads through the scheduler (`sat_music_info(...).disc_streamed` says so); anything else (a RAM blob,
another reader) keeps the old path, and the drive plays one disc-scheduled track at a time. A
second `sat_music_open` becomes the *standby* (see "Prefetch and gapless handoff"); a third returns
`SAT_ERR_BUSY`. The invariants this keeps:

- one 16 KiB segment (ADPCM: whole sectors near 8 blocks) is in flight at a time, and a second one
  is delivered while the next is read; at most two are started and not yet in the rings, and the
  feed only names a read when both rings have room for every frame those segments can still add, so
  a ring write never fails and nothing waits;
- a frame pays one polled batch (~4 ms) plus one delivery slice (4 KiB of PCM, or one ADPCM block,
  ~1.8 ms for S16 stereo), never a whole segment;
- seek, stop and close drop the segments read for the old position (`sat_disc_stream_music_abort`),
  waiting at most one segment of drive time; pause leaves the segments already started to land;
- `sat_music_play` reads nothing: it marks the track wanted and holds both streams paused while
  `sat_music_update` / `sat_disc_stream_service` fill the rings (`sat_music_info(...).starting` is 1
  meanwhile; the jukebox on Mednafen took 87 display frames, about 1.5 s, to read and deliver the 1.5 s
  rings and write the opening halves; an emulator figure, not a hardware one), then releases both streams
  together. The music is silent until then, and a program that never calls `sat_music_update` hears
  nothing;
- a data job queued on the same scheduler (`sat_disc_stream_queue`) takes the gaps while the rings
  hold the reserve, exactly as with the built-in feed;
- look every file up before the music plays, and keep progress hooks from calling
  `sat_music_update`: a lookup or a file read finishes the segment in flight.

### Prefetch and gapless handoff

The second disc track opened waits behind the playing one. `sat_music_prefetch` lets the scheduler
fill its rings silent; `sat_music_play` on it then swaps the two with no refill, and the old track
rewinds to 0 as the new standby (`sat_music_info(...).standby` / `.prefetched`).

- The standby is only read when the playing track's rings are well stocked: once its buffer reaches
  `SAT_DISC_STANDBY_HIGH_FRAMES` (28672) the scheduler reads the standby in a burst of consecutive
  segments, and returns to the playing track when it falls below `SAT_DISC_STANDBY_FLOOR_FRAMES`
  (24576). The playing rings therefore stay above the music reserve; the cost is the gap between those
  two marks plus two segments of margin. A standby is `prefetched` at 16384 frames; a swap before that
  still works, it only shortens the refill.
- A swap drops the old primary's delivered segments at once and marks its in-flight read to be
  discarded when it lands (nothing waits for the drive).
- Host tests (`test_music_disc`) assert the invariants: min playing buffer >= reserve during bursts,
  at most two switches per burst, `fly_discard` on a mid-read swap, seek and mid-segment swaps.
- Emulator measurement (Mednafen, jukebox, 22.05 kHz stereo S16 tracks): the standby reached 20072
  frames and "NEXT READY" after ~90 s with 0 underruns and worst frame 21 ms. That is slow because the
  scheduler's per-segment pacing (~180 ms per segment on that emulator) leaves only a few percent of
  drive throughput beyond one such track; it is an emulator figure, not a hardware one. Ymir reached it
  in a few seconds.
- **Default: only while the playing track is paused or stopped.** `sat_music_update` keeps the
  standby's gate closed while a track plays (`sat_disc_stream_music_standby_gate`), so a prefetch asked
  for behind a playing track waits until it is paused or stopped, and a slow drive cannot starve the
  music. The host model with 1.2 s seeks: prefetch requested while playing: 0 reads of the standby, 0
  underruns; after a pause the prefetch completes and the swap is immediate.
  `sat_music_set_prefetch_while_playing(1)` opens the gate (the jukebox does), which is the mode the
  rest of this section describes.
- **Adaptation (with the gate open):** the scheduler watches the playing rings through each visit to the standby's file
  (their fill is the clock, so it needs no timer). A visit that left them under the reserve raises
  `standby_high` and `standby_floor` by the shortfall for the next visits; one that no ring depth can
  cover (`SAT_DISC_STANDBY_CEIL_FRAMES`) stops the prefetch (`standby_disabled`), so a slow drive costs one
  visit instead of repeating it. `stats.standby_visits` / `standby_worst_dip` report it. A host model of
  the manual's figures (`test_prefetch_seek_cost`: a seek charged on every file switch; a model, not a
  measurement) gives: seeks free, the policy never adapts; 500 ms, high 28672 -> 30549, no underrun, the
  prefetch completes; 1.2 s, the first visit starves the playing track (134 underruns) and the prefetch
  stops. With the playing track paused the same 1.2 s drive completes the prefetch
  with no underrun and the swap is immediate (the model's safe path, also in the test).
- **Real drive:** the prefetch moves the laser between two files. The manual's average seek (500 ms at
  2x) is about the playing ring's depth, so prefetch while the playing track is paused or fading, or
  where a glitch is acceptable. Emulators model seeks as free and cannot show this.
- Limits: two disc tracks use both of the library's two music slots; the old track restarts from 0
  (no resume position); no crossfade.

Measured on the jukebox (Mednafen, 60 s): worst `sat_music_update` 14.6 ms (4.3 ms poll, 1.8 ms
delivery, and the audio driver's own refill when it fell in the same call), worst frame 31 ms,
0 underruns, rings never below 24,320 frames. Ymir, with the harness acceptance test: same
SCSP contract, L/R in phase, only the inactive half rewritten, including a 1100-frame run with
seeks and pause. `tests/host/test_music_disc.cpp` runs the real `sat_music` and scheduler against a
fake drive and SCSP rings: bytes over loops for every format, seeks, aborts, ownership, data jobs
beside music and transport errors.

### The SCSP refill is always spread

The remaining large cost in a frame is not the CD but the audio driver's own refill: when the SCSP
moves into one half of the Sound RAM loop, `sat_audio_update` rewrites the other half (4096 samples
per channel) in one call, about 12 ms for a stereo pair on Mednafen. The manuals set a floor for
this: the SH-2 may only use word or longword accesses to Sound RAM (SCSP manual p01_10, byte access
prohibited), and the FAQ gives 256 bytes from Work RAM-H to the SCSP in 78.14 us by program, 77.74 us
by CPU-DMA and 63.89 us by SCU-DMA, so a 16 KiB stereo-pair refill is about 5 ms at best, in one
frame. The driver therefore writes the inactive half in slices of 256 frames over the display frames
the half lasts (186 ms at 22.05 kHz), keeping three frames spare, and the two opening halves in slices
of 1024 frames per call before the voice is keyed on (about eight updates):

- a slice is the part of the half owed by this frame, so no `sat_audio_update` pays more than a
  fraction of the rewrite; a half shorter than four display frames is still written in one call;
- the ring is consumed only when the half is whole, so a failed upload leaves it intact and the
  refill restarts; buffered frames lag by up to one half meanwhile;
- if the half changes again while a refill is unfinished, the rest is written at once and an
  underrun is counted;
- it applies to every stream and cannot be switched off; a stream starts a few display frames after
  `sat_audio_stream_resume` instead of inside the call that follows it.

Measured on the jukebox (Mednafen, 60 s): worst audio hook 1.15 ms (was 11.8 ms),
worst `sat_music_update` 6.0 ms (was 14.6 ms), worst frame 20 ms (was 31 ms), 0 underruns, rings
never below 24,320 frames. The Ymir acceptance runs (300 and 1100 frames with seeks and pause) pass.

## Resident 68000 scores

For music built from a small resident sample bank, the sound driver can run a
sorted score of up to 1024 SCSP register writes in Sound RAM. The SH-2 uploads
the samples and score once; timer A then advances the score on the SCSP 68000
every 256 output samples. `sat_sound_driver_score_load` starts a looping pass
two ticks after publication, and the 68000 repeats it using the requested
loop length. `sat_sound_driver_info` reports the current event index, completed
loops and writes performed. This path has no steady-state CD traffic or PCM
decoding. The SCSP still renders the short resident PCM waveforms; the 68000
controls their timing, pitch and level.

The dino demo defaults to this path and keeps its complete ADPCM recording as
an alternate build (`DINO_MUSIC_68K=0`). Its resident timbres are short clips
from that recording, converted and loop-crossfaded by the generic audio build
step. The small bank is read from CD only during startup.

The Killer Clown fight uses resident PCM8 effects with `SAT_SOUND_PLAY_AT_TICK`.
Each request is queued one timer tick ahead; the 68000 performs key-on while
the SH-2 renders. The double bell is one waveform with two strikes, so no
far-future event blocks subsequent combat sounds in the FIFO event ring.
The timed-play facade checks ring capacity before reconfiguring or stealing
a voice; a full queue preserves the old hardware state and voice handle.
This relies on the SH-2 being the sole event producer and the 68000 consuming.

## Scheduler / hardware phase rule

Steady-state double-buffer phase is owned by the SCSP, not by VBlank math.
LibSaturn selects the streaming slot with `MSLC` and reads `CA`; because one
CA unit is 4096 samples, `CA & 1` directly identifies the active 4096-sample
half. A refill happens only after CA has moved to the other half, so the old
half is known to be hardware-inactive before the SH-2 writes it.

The MSLC/CA monitor is latched by the SCSP slot pipeline. The HAL therefore
allows at least one 44.1 kHz sample period after changing MSLC before trusting
CA. The stream service samples CA at most once per display-service frame; this
also prevents synchronous CD polling from hammering the monitor register.

`sat_audio_update()` is still deliberately pumped while synchronous CD
commands are waiting. Display-frame accounting remains relevant for one-shot
voice lifetime and for bounding how often the stream service runs, but it is no
longer used to predict which SCSP half is active. A catch-up or duplicated
VBlank therefore cannot by itself select the wrong half for a refill.

When changing audio timing, explicitly test this sequence:

1. start a long stereo stream;
2. enter a synchronous CD wait that spans at least one VBlank;
3. call/pump `sat_audio_update()` during the wait;
4. resume the normal frame loop;
5. verify refill count changes only when the observed CA half changes;
6. verify only the inactive Sound RAM half changes;
7. verify `underrun_count == 0`.

## Harness strategy

The harness should prefer hardware state over screenshots for audio work.

Useful assertions, in increasing strength:

1. guest-visible stream stats: playing, buffered frames, refill count and
   underrun count;
2. SCSP slot configuration: SA/LSA/LEA, PCM8B, LPCTL, OCT/FNS, TL, DISDL and
   DIPAN;
3. SCSP current sample position (`currSample` in Ymir, equivalent to the
   hardware CA concept at coarse granularity);
4. sound-RAM snapshots around a refill boundary, proving that only the inactive
   half changed;
5. captured audio samples, if/when the harness grows an audio-output capture
   path.

Ymir exposes SCSP slot state through `saturn.SCSP.GetProbe().GetSlots()`.
The LibSaturn probe serializes slots 28-31 under `scsp.stream_slots` in
`probe.json`, including:

- `active`, `key_on`, `pcm8`;
- `start_address`, `loop_start`, `loop_end`, `curr_sample`;
- `loop_control`, `octave`, register-visible `fns`;
- `total_level`, `direct_send_level`, `direct_pan`;
- FNV-1a hashes of both 4096-sample Sound RAM halves when the slot matches the
  continuous S16 streaming layout.

Pass `--scsp-trace` to the probe, or `-ScspTrace` to `run-harness.ps1`, to
capture those fields after every program frame under `scsp.trace`. The wrapper
enables this automatically for `cd_streaming_jukebox`.

The jukebox acceptance test compares consecutive half hashes. Once a stream is
active, a refill may change only one half, and the changed half must be the
opposite of the half containing `curr_sample`. This directly catches an
active-half overwrite without depending on audible output or screenshots.

## Distortion triage

Use the symptom to narrow the layer before changing code:

- **harsh noise from the first sample:** check byte order and PCM8B;
- **clean but wrong pitch/tempo:** check OCT/FNS and the staged sample rate;
- **periodic buzz/bursts around buffer cadence:** check active-half overwrite,
  refill timing and double-counted clocks;
- **clean audio that breaks only when more voices are mixed:** check DISDL,
  EFSDL and per-side mixer headroom;
- **click exactly at a loop boundary:** check LSA/LEA and waveform continuity.

Keep fixes at the layer that owns the violated invariant. Do not compensate for
a scheduler bug by lowering volume, changing pitch, or re-encoding the asset.
