# SCSP Audio — M68k Driver and SH2 API

## Audio Architecture

```
SH2 Master
   │  (write to Sound RAM 0x05A01000 = communication area)
   ▼
Sound RAM (512 KB at 0x05A00000)
   │  (M68k reads commands from this area)
   ▼
M68k (11.3 MHz) — audio driver runs here
   │
   ▼
SCSP (Yamaha custom DSP + 32 PCM/FM channels)
   │
   ▼
Stereo audio output (DAC)
```

**SH2 does not control SCSP directly** — all communication is via M68k.

## Audio Memory (Sound RAM)

```
0x05A00000 – 0x05A7FFFF  Sound RAM (512 KB)
  Accessible by SH2 (via A-bus, slow: ~100ns)
  Accessible by M68k (native memory)
  
Recommended layout:
  0x05A00000 – 0x05A003FF  M68k driver code
  0x05A00400 – 0x05A007FF  M68k driver continuation
  0x05A01000 – 0x05A0107F  SH2 ↔ M68k communication area (128 bytes)
  0x05A01080 – 0x05A7FFFF  PCM samples (PCM data pool)
```

## SCSP Registers (M68k sees at 0x25B00000)

```
Base: 0x25B00000 (M68k address for SCSP)

Channel N (N = 0..31):
  0x25B00000 + N*0x20 + 0x00  SA_HIGH  — Sample address (bits 19:16) + control flags
  0x25B00000 + N*0x20 + 0x02  SA_LOW   — Sample address (bits 15:0)
  0x25B00000 + N*0x20 + 0x04  LSA      — Loop start address
  0x25B00000 + N*0x20 + 0x06  LEA      — Loop end address
  0x25B00000 + N*0x20 + 0x08  EG       — Envelope (AR/DR/SR/RR)
  0x25B00000 + N*0x20 + 0x0A  FM/OCT   — Frequency: (OCT << 11) | FNS
  0x25B00000 + N*0x20 + 0x0C  PHASE    — Phase modulation
  0x25B00000 + N*0x20 + 0x10  AM/LFO   — AM, LFO
  0x25B00000 + N*0x20 + 0x14  (pitch LFO)
  0x25B00000 + N*0x20 + 0x18  VOL/PAN  — Volume (bits 6:0) + Pan (bits 4:0 in high byte)

SA_HIGH bits:
  Bit 15:  KYONEX — Key on execute (write 1 to start playback)
  Bit 14:  KYONB  — Key on bit
  Bits 11:10: SBCTL — Sound Bypass Control
  Bit 9:   SSCTL1 } Sample type:
  Bit 8:   SSCTL0 }   00=PCM 16-bit, 01=PCM 8-bit, 10=ADPCM
  Bits 7:6: LPCTL — Loop control: 00=no loop, 01=loop, 10=reverse loop
  Bits 3:0: SA bits 19:16 (sample address, upper nibble)
```

## Calculating Frequency

```
freq_word = (sample_rate × 1024) / 44100

Examples:
  44100 Hz → freq_word = 1024 (OCT=7, FNS=0x200)
  22050 Hz → freq_word = 512
  11025 Hz → freq_word = 256

Encoding in FM/OCT register:
  Bits 15:11 = OCT (octave: 0-7, where 7 = highest pitch)
  Bits 10:0  = FNS (fine tune)
  
Formula: pitch = 44100 × 2^(OCT-7) × FNS/1024
For simplicity: use a pre-calculated table or formula in the driver.
```

## M68k Assembly Driver (mc68000)

```m68k
; m68k_audio_driver.s
; Loaded into Sound RAM at 0x05A00000
; Compile with m68k-elf-as or SNASM68K

SCSP_BASE   equ $25B00000
SCOMM_BASE  equ $05A01000  ; Communication area

; Commands (byte CMD at SCOMM_BASE+0)
CMD_PLAY_PCM  equ 1
CMD_STOP      equ 2
CMD_SET_VOL   equ 3
CMD_PLAY_BGM  equ 4

    org $00000000   ; Start of Sound RAM

;; ─── Initialization ──────────────────────────────────────────────────
init_scsp:
    ; Clear all 32 channels (key off)
    lea     SCSP_BASE, a0
    moveq   #31, d0
.clr_ch:
    move.w  #$0000, (a0)    ; SA_HIGH: KYONB=0, KYONEX=0
    add.l   #$20, a0        ; Next channel
    dbra    d0, .clr_ch
    
    ; Master volume to maximum
    move.w  #$7F00, SCSP_BASE+$400  ; Master volume = 127 (maximum)
    
    ; Start polling loop
    bra     main_loop

;; ─── Main Loop ──────────────────────────────────────────────────────
main_loop:
    move.b  SCOMM_BASE, d0      ; Read command byte
    beq     main_loop           ; 0 = idle, continue polling
    
    ; Dispatch based on command
    cmpi.b  #CMD_PLAY_PCM, d0
    beq     do_play_pcm
    cmpi.b  #CMD_STOP, d0
    beq     do_stop
    cmpi.b  #CMD_SET_VOL, d0
    beq     do_set_vol
    
    ; Unknown command: clear and continue
    clr.b   SCOMM_BASE
    bra     main_loop

;; ─── Play PCM ────────────────────────────────────────────────────────
; Parameters in SCOMM:
;   +0x00 (byte) = CMD = 1
;   +0x02 (word) = channel (0..31)
;   +0x04 (word) = sample addr low
;   +0x06 (word) = sample addr high
;   +0x08 (word) = loop start (or 0)
;   +0x0A (word) = loop end (size in samples)
;   +0x0C (word) = frequency word (OCT:FNS)
;   +0x0E (word) = volume/pan (high byte = pan, low byte = vol)
do_play_pcm:
    move.w  SCOMM_BASE+$02, d0  ; channel
    mulu    #$20, d0             ; offset = channel × 32
    lea     SCSP_BASE, a0
    add.l   d0, a0               ; a0 → channel N registers

    ; First: key off (ensure channel is stopped)
    move.w  #$0000, (a0)         ; SA_HIGH: clear key on

    ; Sample address
    move.w  SCOMM_BASE+$04, d1  ; addr_low
    move.w  SCOMM_BASE+$06, d2  ; addr_high (bits 19:16 in low nibble)
    move.w  d1, $02(a0)          ; SA_LOW
    
    ; SA_HIGH: PCM16 sample type, no loop + bits 19:16 of address
    andi.w  #$000F, d2           ; Only bits 3:0 (addr bits 19:16)
    ori.w   #$0040, d2           ; SSCTL=01 (PCM 8-bit) ... or 0x0000 for 16-bit
    move.w  d2, (a0)             ; SA_HIGH (without KYONEX yet)

    ; Loop end (size)
    move.w  SCOMM_BASE+$0A, $06(a0)  ; LEA

    ; Frequency
    move.w  SCOMM_BASE+$0C, $0A(a0)  ; FM/OCT

    ; Volume and pan
    move.w  SCOMM_BASE+$0E, $18(a0)  ; VOL/PAN

    ; Envelope: max attack, min decay/release (direct sound)
    move.w  #$001F, $08(a0)

    ; Key on! (KYONEX = bit 15, KYONB = bit 14)
    ori.w   #$C000, d2
    move.w  d2, (a0)             ; SA_HIGH with KYONEX=1, KYONB=1

    ; Confirm receipt (clear CMD)
    clr.b   SCOMM_BASE
    bra     main_loop

;; ─── Stop channel ────────────────────────────────────────────────────
do_stop:
    move.w  SCOMM_BASE+$02, d0
    mulu    #$20, d0
    lea     SCSP_BASE, a0
    add.l   d0, a0
    move.w  #$0000, (a0)         ; KYONB=0, KYONEX=0 = key off
    clr.b   SCOMM_BASE
    bra     main_loop

;; ─── Set volume ──────────────────────────────────────────────────────
do_set_vol:
    move.w  SCOMM_BASE+$02, d0   ; channel
    move.w  SCOMM_BASE+$04, d1   ; new volume (0..127)
    mulu    #$20, d0
    lea     SCSP_BASE+$18, a0
    add.l   d0, a0
    move.b  d1, 1(a0)            ; Low byte of VOL/PAN
    clr.b   SCOMM_BASE
    bra     main_loop
```

## SH2 API — Side

```cpp
// audio/audio_api.hpp
#pragma once
#include <cstdint>

// Communication area accessed via cache-through (force write to RAM)
#define SCOMM(offset, type) \
    (*reinterpret_cast<volatile type*>((0x05A01000 | 0x20000000) + (offset)))

namespace Audio {
    // Wait for M68k to process previous command
    inline void wait() {
        while (SCOMM(0, uint8_t) != 0);
    }

    // Load PCM sample into Sound RAM
    // Returns address in Sound RAM for use in play()
    uint32_t load_pcm(const void* data, uint32_t bytes,
                      uint32_t sound_ram_offset = 0x10000) {
        auto* dst = reinterpret_cast<volatile uint8_t*>(
            0x20000000 | (0x05A00000 + sound_ram_offset));
        auto* src = static_cast<const uint8_t*>(data);
        for (uint32_t i = 0; i < bytes; ++i) dst[i] = src[i];
        return sound_ram_offset;
    }

    void play_pcm(uint8_t ch, uint32_t sample_addr, uint32_t sample_len,
                  uint16_t freq_word, uint8_t volume = 127, uint8_t pan = 0) {
        wait();
        SCOMM(0x02, uint16_t) = ch;
        SCOMM(0x04, uint16_t) = static_cast<uint16_t>(sample_addr & 0xFFFF);
        SCOMM(0x06, uint16_t) = static_cast<uint16_t>(sample_addr >> 16);
        SCOMM(0x08, uint16_t) = 0;  // Loop start = 0
        SCOMM(0x0A, uint16_t) = static_cast<uint16_t>(sample_len);
        SCOMM(0x0C, uint16_t) = freq_word;
        SCOMM(0x0E, uint16_t) = static_cast<uint16_t>((pan << 8) | volume);
        SCOMM(0x00, uint8_t)  = 1;  // CMD = PLAY_PCM (triggers M68k)
    }

    void stop(uint8_t ch) {
        wait();
        SCOMM(0x02, uint16_t) = ch;
        SCOMM(0x00, uint8_t)  = 2;  // CMD = STOP
    }

    // Calculate freq_word for a desired sample rate
    constexpr uint16_t freq_word(uint32_t sample_rate) {
        // Approximation: freq_word ≈ sample_rate × 1024 / 44100
        return static_cast<uint16_t>((static_cast<uint32_t>(sample_rate) * 1024) / 44100);
    }
}
```

## Compiling the M68k Driver

```bash
# M68k toolchain for the audio driver
# GCC for m68k (only for verification; production uses SNASM68K or m68k-elf-as)

# Option 1: GNU Assembler
m68k-linux-gnu-as -m68000 -o m68k_driver.o m68k_driver.s
m68k-linux-gnu-objcopy -O binary m68k_driver.o m68k_driver.bin

# Option 2: Compile and embed in SH2 binary as const array
xxd -i m68k_driver.bin > m68k_driver_data.c

# Copy to Sound RAM at game boot:
extern const uint8_t m68k_driver_bin[];
extern const uint32_t m68k_driver_bin_len;

void audio_init() {
    auto* sram = reinterpret_cast<volatile uint8_t*>(0x20000000 | 0x05A00000);
    for (uint32_t i = 0; i < m68k_driver_bin_len; ++i)
        sram[i] = m68k_driver_bin[i];
    // M68k starts executing automatically when released
    // The BIOS already starts M68k; just have the driver at the start of Sound RAM
}
```

## CDDA (Digital CD Music)

CDDA plays directly from CD → audio output, CPU cost = zero.

```cpp
// Control via CD block (more complex command system)
// Requires sending commands to SH1 (CD processor) via I/O area

// Simplified: use the SBL CD library for CDDA
// Or implement via direct command to CD block:
#define CD_BLOCK_REG(n) (*reinterpret_cast<volatile uint16_t*>(0x25890000 + (n)*2))

void cdda_play_track(uint8_t track) {
    // CD Block command sequence for CDDA playback:
    // 1. Set Mode (CD Audio mode)
    // 2. Play (with track number)
    // See: antime.kapsi.fi/sega/docs.html → CD Library docs
}
```
