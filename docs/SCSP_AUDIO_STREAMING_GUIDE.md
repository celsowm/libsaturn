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

### ADX music assets (experimental)

LibSaturn supports CRI ADX *encoding 3* (18-byte frame / 32 samples / channel)
through `sat_music` and the existing disc scheduler. Unlike PCM8, this is **not
hardware decoding**: the SH-2 expands ADX to channel-planar signed S16BE,
which the SCSP plays with its ordinary stereo stream slots. The 68000 driver
is not involved in ADX decoding.

Use `tools/convert_audio.py --format adx` rather than placing a raw `.adx`
file in the disc image. The generated LibSaturn payload consists of two
big-endian signed Q12 LPC coefficients (4 bytes) followed by normal CRI ADX
18-byte frames, one per channel in each 32-sample group. The CRI container
header and 18-byte EOF marker are removed. The emitted header declares
`SAT_AUDIO_ADX_ADPCM`, sample rate, channel count, PCM frame count, and
payload size. At 22050 Hz stereo the compressed rate is 24,806 bytes/s;
it is not the same as the 32,768-frame-per-channel decoded ring budget.

The predictor is continuous between ADX groups and reset when a track loops.
The last group's padding is not played. Seeking to a nonzero frame is currently
**unsupported** for ADX: safe random seek needs a stored predictor checkpoint
or re-decoding history. Stopping/restarting at zero is supported. Like IMA,
the current `sat_music` implementation retains its large Work RAM Low rings
(~288 KiB of allocated music/runtime buffers); this implementation does
**not** yet solve that memory cost.

To prepare an asset (use a legally obtained input):

```sh
python3 tools/convert_audio.py --input source.wav \
  --output-audio build/generated/theme.adxbin \
  --output-header build/generated/theme_adx.h \
  --name theme_adx --sample-rate 22050 --channels 2 --format adx
```

Register the emitted payload as a `SAT_ASSET_STREAM` with
`format = SAT_AUDIO_ADX_ADPCM`, the generated `sample_count` and
`sample_rate`, and either a memory pointer or a CD source path.
Call `sat_music_use_disc_stream` before playback when using the asynchronous
CD scheduler. The PCM16 and IMA paths remain unchanged.

At this stage the reference decoder has host tests against FFmpeg-produced
frames, but physical Saturn throughput, seeks and collision with combat SFX
are not validated. An optimized 68000 ADX decoder remains a separate project.

### Compact ADX CD player (caller-owned rings)

For memory-constrained games, `saturn/adx.h` exposes `sat_adx_player_*`,
independent of the larger `sat_music` runtime. The linker only pulls in the
player that the application uses; `sat_adx_player_*` does not link the
`sat_music` implementation's approximately 288 KiB of static Work RAM Low.
The new API requires a primary `sat_disc_stream_t` feed and a previously
resolved `sat_cdfs_file_t`. It does not heap-allocate, use CD-DA, or
interfere with resident sound effects.

The application provides `sat_adx_pcm_rings_t`: two 16384-frame mono S16BE
rings (64 KiB total for stereo), plus `sat_adx_player_t` with 8 KiB planar
decode staging and small state. The disc scheduler already owns 32 KiB of
segment storage. The SCSP streaming runtime owns 32 KiB of upload staging,
and the two playback slots reserve 32 KiB of **Sound RAM** (distinct from
Work RAM). The exact linked RAM total also depends on the application's
other linked facilities.

Generate assets with `tools/convert_audio.py --format adx`. Its header
provides `SAMPLE_RATE`, `SAMPLE_COUNT`, `CHANNELS`, `BYTES`,
`COEFF_1` and `COEFF_2` macros. The final two are the signed LPC Q12
coefficients needed for `sat_adx_config_t`, so opening the file requires no
blocking disc read to obtain the ADX header.

Example outline (after `sat_audio_init`, CD mount and disc stream init):

```c
static sat_adx_pcm_rings_t rings __attribute__((section(".wram_l"),aligned(4)));
static sat_adx_player_t player __attribute__((section(".wram_l"),aligned(4)));
// Obtain file from CDFS and fill config from the generated header.
sat_adx_config_t config = {sample_rate, sample_count, coeff_1, coeff_2,
                           2u, {0, 0, 0}};
sat_adx_player_open(&player, &disc, &file, &config, &rings);
sat_adx_player_play(&player);
// Once each display frame, before sat_audio_update:
sat_adx_player_update(&player);
sat_audio_update();
```

Build a runnable example with `make EXAMPLE=adx_compact_cd`. It downloads
CC0 source audio and makes an ADX file for the ISO, mixing it with a resident
PCM16 effect. A = effect, C = pause/resume. The HUD shows the decoded frame
count, ring fill, underruns, and delivery cost.

The compact feed pipelines two sectors (4096 bytes) per read, reassembles
ADX frames split at sector boundaries, limits delivery to 2048 decoded frames
per update, and preserves predictor history across frames. At the final ADX
group it skips the padding and resets LPC history before the music loops.
Calling `stop` aborts old CD requests and resets both rings and predictors.

**Limitations:** ADX still decodes on the SH-2, *not* the SCSP's Motorola
68000. This API currently offers full-file looping only, and one primary
compact ADX feed per scheduler; it cannot share that primary with
`sat_music` or the scheduler's built-in raw music feed. The 16384-frame
rings offer limited tolerance to drive seeks, so actual performance must be
measured in Ymir/Mednafen and on real Saturn hardware before using this
path during a combat scene. Do not infer real-drive seek timings from
emulation results.

### Seamless PCM8

`SAT_AUDIO_PCM_S8` takes the same continuous path as S16. A half is still
4096 *samples*, so the byte size depends on the format; the halves stay
contiguous because the loop (`LSA=0`, `LEA=8191`, 8192 samples) is one run of
samples:

| Format | PCM8B | Bytes per half | Half 1 starts at | Loop bytes |
| --- | --- | --- | --- | --- |
| S16 | 0 | 8192 | `SA + 8192` | 16384 |
| S8 | 1 | 4096 | `SA + 4096` | 8192 |

The stride is the logical one (`half * 4096 * bytes_per_sample`). The slot's
Sound RAM *reservation* is unchanged, `2 * 8192` bytes at the top of sound
RAM, so allocations never overlap and S16 and S8 streams can be mixed. A PCM8
stream simply leaves the upper half of its reservation unused. CA still counts
samples, so the half detection is the same for both formats.

- Supported rates: any rate up to `4096 * display_rate / 2` (122.88 kHz at
  60 Hz, 51.2 kHz at 25 Hz); 22.05, 32 and 44.1 kHz are covered in the host test.
- Stereo is two mono S8 streams, hard-panned and keyed on by one KYONEX, as
  for S16. Feed both with the same number of frames.
- Work RAM: no new static memory. The caller owns each ring (1 byte per
  frame, so 16 KiB for 16384 frames, half of S16); the library keeps its
  existing 8 KiB staging buffer per slot. Sound RAM used per stream: 8 KiB.
- Refill safety is unchanged: only the half CA is not in is written, a slice
  per service call, and the ring is consumed once the half is whole.
  `pause` keys the voice off and `resume` reopens the loop from the ring, which
  must again hold two halves.
- A 4096-sample half at 22.05 kHz lasts 186 ms (about 11 frames at 60 Hz). The
  rate of 8-bit audio is 1 byte per sample per channel (44.1 KB/s stereo at
  22.05 kHz).
- Limits: 8-bit PCM has about 48 dB of dynamic range (audible hiss on quiet
  passages). The service must still run at least once per two frames of
  the half's duration; synchronous CD reads longer than that stall it.
  Host tests simulate the SCSP; behaviour on a physical Saturn is not verified.

Example, 22.05 kHz signed PCM8 stereo from two mono streams:

```c
sat_audio_spec_t spec = {22050u, 16384u, 1u, SAT_AUDIO_PCM_S8, 0u};
static int8_t left_ring[16384], right_ring[16384];
sat_audio_stream_t left, right;
sat_audio_stream_open(&left,  &spec, left_ring,  sizeof left_ring);
sat_audio_stream_open(&right, &spec, right_ring, sizeof right_ring);
sat_audio_stream_set_pan(left,  SAT_AUDIO_PAN_LEFT);
sat_audio_stream_set_pan(right, SAT_AUDIO_PAN_RIGHT);
/* prime two halves (8192 frames) on both before the first service call */
sat_audio_stream_write(left,  left_pcm,  8192u);
sat_audio_stream_write(right, right_pcm, 8192u);
/* each frame: top the rings up with whatever the producer has, then the
 * library's per-frame audio service runs the refill */
```

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

### Paged scores (driver version 3)

A score longer than 1024 writes uses the same 8 KiB score area as two pages of 512 events.
`sat_sound_driver_pages_start` begins a stream; the SH-2 calls `sat_sound_driver_page_publish` whenever
`sat_sound_driver_pages_info` shows a free page, and only the 68000 consumes: it plays the pages in number order
and moves to the next in the same tick, so a note-dense score runs while the SH-2 does nothing but copy pages.
Event times are absolute stream time (stream base + page time base + event tick), so nothing drifts. A page is
published by one word write after its data, count and times and is never rewritten until the 68000 frees it
(`SAT_ERR_BUSY` otherwise). The driver counts late events, underruns (the next page missing after the previous
one's end time) and out-of-order pages. The 1024-event score and the paged score exclude each other.

`tick_samples` 128 halves the tick (2.902 ms): timers A and B free-run at 256 samples, started 128 apart, and
each interrupt is a tick, because the timers wrap to 0 rather than reload, so a preload cannot shorten the
period. Measured on Ymir, the 68000 runs about 30 events per millisecond: 176 events in a 256-sample tick and 80
in a 128-sample tick are clean; 192 and 96 start to run late and lose ticks. `examples/sound_driver_pages_demo`
plays 1,600 generated events in 4 pages with no late event.

The dino demo can select this path with
`DINO_MUSIC_CDDA=0 DINO_MUSIC_68K=1` and keeps its complete ADPCM recording as
another alternate build (`DINO_MUSIC_CDDA=0 DINO_MUSIC_68K=0`). Its resident timbres are short clips
from that recording, converted and loop-crossfaded by the generic audio build
step. The small bank is read from CD only during startup.

The Dino's current default is a separate CD-DA path for the user's clean Demo 1
music reference. It launches the mixed-mode CUE, obtains the audio range from
the TOC once, and repeats that explicit FAD range. Its stereo CD samples enter
SCSP EXTS0/1; the low-byte EFSDL/EFPAN controls at slot 16/17 register 0x16 route
left/right at unity (`0xFF` and `0xEF`) while preserving the unrelated upper
byte. This follows the mixer manual linked above. No PCM slot, refill ring or
68000 score participates. Do all filesystem reads before starting this path:
a later data read would move the same drive away from the music. Verify this
path by comparing captured PCM with the staged CD-DA bytes and checking the
external-input mixer registers, rather than applying the streaming-slot table.

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

### PCM8 music from the CD

`examples/pcm8_cd_music` plays a 22.05 kHz signed 8-bit stereo track that exists only in the ISO: the disc
scheduler feeds `sat_music`, which drives the two seamless PCM8 streams, while a resident effect plays over it.
Measured in Mednafen over 34 s (one loop wrap): 0 underruns, 206 refills, at least 20992 frames buffered,
recording correlated 0.998 with the source at every 1 s probe after start. Real hardware is not validated.
