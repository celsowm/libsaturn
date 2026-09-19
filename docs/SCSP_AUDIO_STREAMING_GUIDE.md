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

## Scheduler clock rule

Streaming phase is estimated from elapsed display time, so every time source
used by the refill scheduler must represent the same physical interval exactly
once.

`sat_audio_update()` is deliberately pumped while synchronous CD commands are
waiting. During that wait the normal application frame path can be stalled, so
LibSaturn also observes VBlank edges to keep audio moving. When normal frame
counting catches up afterward, those already-observed display frames must be
**reconciled**, not added again.

Double-counting that interval advances `completed_chunks` too far and can make
the runtime refill the half that the SCSP has not finished yet. That failure is
much more destructive than an ordinary underrun because valid PCM is replaced
while the hardware is reading it.

When changing audio timing, explicitly test this sequence:

1. start a long stereo stream;
2. enter a synchronous CD wait that spans at least one VBlank;
3. call/pump `sat_audio_update()` during the wait;
4. resume the normal frame loop;
5. verify no refill-count jump corresponding to the same display frames twice;
6. verify `underrun_count == 0`.

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

Ymir exposes SCSP slot state through `saturn.GetSCSP().GetProbe().GetSlots()`.
That is preferable to reconstructing slot state from unrelated video behavior.
The existing harness does not yet serialize SCSP slots into `probe.json`, so a
future harness extension should expose at least:

- `active`, `keyOnBit`;
- `startAddress`, `loopStartAddress`, `loopEndAddress`;
- `currSample`;
- `pcm8Bit`, `loopControl`;
- `octave`, `freqNumSwitch`;
- `totalLevel`, `directSendLevel`, `directPan`.

For streaming tests, sampling those fields at several frames is more valuable
than a single final snapshot.

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
