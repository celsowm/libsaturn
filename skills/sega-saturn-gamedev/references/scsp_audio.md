# SCSP Áudio — Driver M68k e API SH2

## Arquitetura de Áudio

```
SH2 Master
  │  (escrita em Sound RAM 0x05A01000 = área de comunicação)
  ▼
Sound RAM (512 KB em 0x05A00000)
  │  (M68k lê comandos desta área)
  ▼
M68k (11.3 MHz) — driver de áudio roda aqui
  │
  ▼
SCSP (Yamaha custom DSP + 32 canais PCM/FM)
  │
  ▼
Saída de áudio estéreo (DAC)
```

**SH2 não controla SCSP diretamente** — toda comunicação é via M68k.

## Memória de Áudio (Sound RAM)

```
0x05A00000 – 0x05A7FFFF  Sound RAM (512 KB)
  Acessível pelo SH2 (via A-bus, lento: ~100ns)
  Acessível pelo M68k (memória nativa)
  
Layout recomendado:
  0x05A00000 – 0x05A003FF  Código do driver M68k
  0x05A00400 – 0x05A007FF  Driver M68k continuation
  0x05A01000 – 0x05A0107F  Área de comunicação SH2 ↔ M68k (128 bytes)
  0x05A01080 – 0x05A7FFFF  Amostras PCM (PCM data pool)
```

## Registradores SCSP (M68k vê em 0x25B00000)

```
Base: 0x25B00000 (endereço do M68k para SCSP)

Canal N (N = 0..31):
  0x25B00000 + N*0x20 + 0x00  SA_HIGH  — Endereço sample (bits 19:16) + flags de controle
  0x25B00000 + N*0x20 + 0x02  SA_LOW   — Endereço sample (bits 15:0)
  0x25B00000 + N*0x20 + 0x04  LSA      — Loop start address
  0x25B00000 + N*0x20 + 0x06  LEA      — Loop end address
  0x25B00000 + N*0x20 + 0x08  EG       — Envelope (AR/DR/SR/RR)
  0x25B00000 + N*0x20 + 0x0A  FM/OCT   — Frequência: (OCT << 11) | FNS
  0x25B00000 + N*0x20 + 0x0C  PHASE    — Modulação de fase
  0x25B00000 + N*0x20 + 0x10  AM/LFO   — AM, LFO
  0x25B00000 + N*0x20 + 0x14  (pitch LFO)
  0x25B00000 + N*0x20 + 0x18  VOL/PAN  — Volume (bits 6:0) + Pan (bits 4:0 no high byte)

SA_HIGH bits:
  Bit 15:  KYONEX — Key on execute (escrever 1 para iniciar reprodução)
  Bit 14:  KYONB  — Key on bit
  Bits 11:10: SBCTL — Sound Bypass Control
  Bit 9:   SSCTL1 } Tipo de sample:
  Bit 8:   SSCTL0 }   00=PCM 16-bit, 01=PCM 8-bit, 10=ADPCM
  Bits 7:6: LPCTL — Loop control: 00=sem loop, 01=loop, 10=reverse loop
  Bits 3:0: SA bits 19:16 (endereço sample, upper nibble)
```

## Calculando Frequência

```
freq_word = (sample_rate × 1024) / 44100

Exemplos:
  44100 Hz → freq_word = 1024 (OCT=7, FNS=0x200)
  22050 Hz → freq_word = 512
  11025 Hz → freq_word = 256

Encoding em FM/OCT register:
  Bits 15:11 = OCT (octave: 0-7, onde 7 = pitch mais alto)
  Bits 10:0  = FNS (fine tune)
  
Fórmula: pitch = 44100 × 2^(OCT-7) × FNS/1024
Para simplicidade: usar tabela pré-calculada ou fórmula direta no driver.
```

## Driver M68k Assembly (mc68000)

```m68k
; m68k_audio_driver.s
; Carregado em Sound RAM em 0x05A00000
; Compilar com m68k-elf-as ou SNASM68K

SCSP_BASE   equ $25B00000
SCOMM_BASE  equ $05A01000  ; Área de comunicação

; Comandos (byte CMD em SCOMM_BASE+0)
CMD_PLAY_PCM  equ 1
CMD_STOP      equ 2
CMD_SET_VOL   equ 3
CMD_PLAY_BGM  equ 4

    org $00000000   ; Início do Sound RAM

;; ─── Inicialização ──────────────────────────────────────────────────
init_scsp:
    ; Limpar todos os 32 canais (key off)
    lea     SCSP_BASE, a0
    moveq   #31, d0
.clr_ch:
    move.w  #$0000, (a0)    ; SA_HIGH: KYONB=0, KYONEX=0
    add.l   #$20, a0        ; Próximo canal
    dbra    d0, .clr_ch
    
    ; Master volume ao máximo
    move.w  #$7F00, SCSP_BASE+$400  ; Master volume = 127 (máximo)
    
    ; Iniciar polling loop
    bra     main_loop

;; ─── Loop principal ─────────────────────────────────────────────────
main_loop:
    move.b  SCOMM_BASE, d0      ; Ler byte de comando
    beq     main_loop           ; 0 = idle, continuar polling
    
    ; Dispatch baseado no comando
    cmpi.b  #CMD_PLAY_PCM, d0
    beq     do_play_pcm
    cmpi.b  #CMD_STOP, d0
    beq     do_stop
    cmpi.b  #CMD_SET_VOL, d0
    beq     do_set_vol
    
    ; Comando desconhecido: limpar e continuar
    clr.b   SCOMM_BASE
    bra     main_loop

;; ─── Play PCM ────────────────────────────────────────────────────────
; Parâmetros em SCOMM:
;   +0x00 (byte) = CMD = 1
;   +0x02 (word) = channel (0..31)
;   +0x04 (word) = sample addr low
;   +0x06 (word) = sample addr high
;   +0x08 (word) = loop start (ou 0)
;   +0x0A (word) = loop end (tamanho em samples)
;   +0x0C (word) = frequency word (OCT:FNS)
;   +0x0E (word) = volume/pan (high byte = pan, low byte = vol)
do_play_pcm:
    move.w  SCOMM_BASE+$02, d0  ; canal
    mulu    #$20, d0             ; offset = canal × 32
    lea     SCSP_BASE, a0
    add.l   d0, a0               ; a0 → registradores do canal N

    ; Primeiro: key off (garantir que canal está parado)
    move.w  #$0000, (a0)         ; SA_HIGH: clear key on

    ; Sample address
    move.w  SCOMM_BASE+$04, d1  ; addr_low
    move.w  SCOMM_BASE+$06, d2  ; addr_high (bits 19:16 nos low nibble)
    move.w  d1, $02(a0)          ; SA_LOW
    
    ; SA_HIGH: sample type PCM16, no loop + bits 19:16 do endereço
    andi.w  #$000F, d2           ; Apenas bits 3:0 (addr bits 19:16)
    ori.w   #$0040, d2           ; SSCTL=01 (PCM 8-bit) ... ou 0x0000 para 16-bit
    move.w  d2, (a0)             ; SA_HIGH (sem KYONEX ainda)

    ; Loop end (tamanho)
    move.w  SCOMM_BASE+$0A, $06(a0)  ; LEA

    ; Frequência
    move.w  SCOMM_BASE+$0C, $0A(a0)  ; FM/OCT

    ; Volume e pan
    move.w  SCOMM_BASE+$0E, $18(a0)  ; VOL/PAN

    ; Envelope: attack máximo, decay/release mínimo (som direto)
    move.w  #$001F, $08(a0)

    ; Key on! (KYONEX = bit 15, KYONB = bit 14)
    ori.w   #$C000, d2
    move.w  d2, (a0)             ; SA_HIGH com KYONEX=1, KYONB=1

    ; Confirmar recebimento (limpar CMD)
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
    move.w  SCOMM_BASE+$02, d0   ; canal
    move.w  SCOMM_BASE+$04, d1   ; novo volume (0..127)
    mulu    #$20, d0
    lea     SCSP_BASE+$18, a0
    add.l   d0, a0
    move.b  d1, 1(a0)            ; Byte baixo de VOL/PAN
    clr.b   SCOMM_BASE
    bra     main_loop
```

## API SH2 — Side

```cpp
// audio/audio_api.hpp
#pragma once
#include <cstdint>

// Área de comunicação acessada via cache-through (forçar escrita para RAM)
#define SCOMM(offset, type) \
    (*reinterpret_cast<volatile type*>((0x05A01000 | 0x20000000) + (offset)))

namespace Audio {
    // Aguardar M68k processar comando anterior
    inline void wait() {
        while (SCOMM(0, uint8_t) != 0);
    }

    // Carregar amostra PCM no Sound RAM
    // Retorna endereço no Sound RAM para usar em play()
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
        SCOMM(0x00, uint8_t)  = 1;  // CMD = PLAY_PCM (dispara M68k)
    }

    void stop(uint8_t ch) {
        wait();
        SCOMM(0x02, uint16_t) = ch;
        SCOMM(0x00, uint8_t)  = 2;  // CMD = STOP
    }

    // Calcular freq_word para uma taxa de sample desejada
    constexpr uint16_t freq_word(uint32_t sample_rate) {
        // Aproximação: freq_word ≈ sample_rate × 1024 / 44100
        return static_cast<uint16_t>((static_cast<uint32_t>(sample_rate) * 1024) / 44100);
    }
}
```

## Compilando o Driver M68k

```bash
# Toolchain M68k para o driver de áudio
# GCC para m68k (apenas para verificação; produção usa SNASM68K ou m68k-elf-as)

# Opção 1: GNU Assembler
m68k-linux-gnu-as -m68000 -o m68k_driver.o m68k_driver.s
m68k-linux-gnu-objcopy -O binary m68k_driver.o m68k_driver.bin

# Opção 2: Compilar e embutir no binário SH2 como array const
xxd -i m68k_driver.bin > m68k_driver_data.c

# Copiar para Sound RAM no boot do jogo:
extern const uint8_t m68k_driver_bin[];
extern const uint32_t m68k_driver_bin_len;

void audio_init() {
    auto* sram = reinterpret_cast<volatile uint8_t*>(0x20000000 | 0x05A00000);
    for (uint32_t i = 0; i < m68k_driver_bin_len; ++i)
        sram[i] = m68k_driver_bin[i];
    // M68k começa a executar automaticamente ao ser liberado
    // O BIOS já inicia o M68k; basta ter o driver no início do Sound RAM
}
```

## CDDA (Música em CD Digital)

CDDA reproduz direto do CD → saída de áudio, custo de CPU = zero.

```cpp
// Controle via CD block (sistema de comandos mais complexo)
// Requer enviar comandos ao SH1 (processador de CD) via área de I/O

// Simplificado: usar o SBL CD library para CDDA
// Ou implementar via comando direto ao CD block:
#define CD_BLOCK_REG(n) (*reinterpret_cast<volatile uint16_t*>(0x25890000 + (n)*2))

void cdda_play_track(uint8_t track) {
    // Sequência de comandos CD Block para reprodução CDDA:
    // 1. Set Mode (CD Audio mode)
    // 2. Play (com número de track)
    // Ver: antime.kapsi.fi/sega/docs.html → CD Library docs
}
```
