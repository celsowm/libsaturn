# SCU DSP — Referência Completa

## Overview

- Arquitetura Harvard, pipeline VLIW
- Clock: 14.31818 MHz (metade do SH2)
- 256 words × 32-bit de Program RAM em 0x05FF8000
- 3 × 64 words × 32-bit de Data RAM (CT0, CT1, CT2 + CT3) em 0x05FF8400
- Acumulador 48-bit interno (AC)
- Até **6 operações paralelas** por ciclo
- Sem divisão de hardware — usar LUT de recíproco
- Assembly-only: nenhum compilador C suporta

## Data RAMs

```
Endereço Base:
  CT0: 0x05FF8400  (64 × 32-bit, ring buffer, pointer auto-increment)
  CT1: 0x05FF8500
  CT2: 0x05FF8600
  CT3: 0x05FF8700

Acesso pelo SH2:
  Leitura/escrita direta (lenta, evitar em loop crítico)
  Ou via SCU DMA Ch2 (preferido)
```

Ring buffer: cada leitura/escrita avança o pointer +1 automaticamente. Wrap em 64 entries.

## Registradores do DSP

| Reg   | Descrição                                       |
|-------|-------------------------------------------------|
| AC    | Acumulador 48-bit (resultado principal)         |
| CT0–CT3 | Ponteiros de ring buffer (6-bit, auto-inc)   |
| P     | Resultado parcial de MUL (antes do MAC)         |
| RA0   | Endereço de leitura DMA                         |
| WA0   | Endereço de escrita DMA                         |
| PC    | Program Counter                                 |
| LOP   | Loop counter                                    |
| TOP   | Topo do loop (endereço para BTM)                |

## Instrução VLIW — Campos Paralelos

Uma palavra de instrução DSP (32-bit) codifica até 6 campos:
```
[D1  ][D2  ][X  ][Y  ][ALU ][CTRL]
 4-bit 4-bit 3-bit 3-bit 4-bit 12-bit
```
- **D1/D2:** Operações de memória (leitura/escrita em Data RAM)
- **X/Y:** Seleção de operandos para a unidade multiplicadora
- **ALU:** Operação no acumulador (MAC, ADD, SUB, etc.)
- **CTRL:** Desvio, fim de programa, DMA

## Set de Instruções

### ALU
```asm
CLR A           ; AC = 0
MOV [src], A    ; AC = src
ADD [s], A      ; AC += s
SUB [s], A      ; AC -= s
AD2 [s], A      ; AC = AC/2 + s  (médio ponderado)
AND [s], A      ; AC &= s
OR  [s], A      ; AC |= s
XOR [s], A      ; AC ^= s
LSR A           ; AC >>= 1 (lógico)
ASR A           ; AC >>= 1 (aritmético)
ABS A           ; AC = |AC|
```

### Multiply-Accumulate (o mais importante)
```asm
MUL [s1], [s2]      ; P = s1 × s2  (não altera AC)
MAC [s1], [s2]      ; AC += s1 × s2
                    ; Operandos podem ser CT0, CT1, CT2, CT3 ou imediato
                    ; Exemplo:
MAC CT0, CT1        ; AC += DataRAM[CT0] × DataRAM[CT1]
                    ; CT0++ e CT1++ automaticamente
```

**Para fixed-point 16.16:**
- Multiplicar dois valores 16.16 gera resultado 32.32 em 48-bit AC
- Os 16 bits de inteiro do resultado estão em AC[31:16]
- Para extrair resultado 16.16: armazenar AC e deslocar

### MOV / Load / Store
```asm
MOV [s], [d]        ; d = s
MOV A, [d]          ; Data RAM[addr] = AC (stores upper 32 bits of 48-bit AC)
MOV [s], CT0        ; CT0 = s (definir pointer)
MVI #imm25, CT0     ; CT0 = immediate (25-bit sign-extended)
MVI #imm25, A       ; AC = immediate
```

### Controle
```asm
NOP                 ; Nenhuma operação (preencher slots vazios)
END                 ; Fim do programa, dispara interrupção
ENDI                ; Fim do programa, NÃO dispara interrupção
JMP [addr]          ; Salto incondicional
BTST [bit], [src]   ; Testar bit
BTT  [addr]         ; Branch if true (após BTST)
BTF  [addr]         ; Branch if false

; Loop (construção de hardware)
LPS                 ; Marcar início do loop (salvo em TOP)
LE                  ; Loop end — decrementa LOP, salta para TOP se LOP > 0
```

## Regras de Paralelismo (CRÍTICO)

1. **MAC** usa os barramentos X e Y → não pode parallelizar com outra instrução que use X ou Y
2. **MOV para/de AC** usa o barramento ALU → não combina com MAC no mesmo ciclo
3. **D1/D2 podem operar simultaneamente** — cada um tem barramento independente
4. **CTRL** (branch/end) ocupa o slot de controle; pode combinar com ALU/MAC

**Exemplo de paralelismo máximo:**
```asm
; Ciclo único executando: MAC + 1 leitura D1 + 1 escrita D2
; Sintaxe Hitachi DSP: múltiplas ops separadas por '|'
MAC CT0, CT1 | MOV [addr], D2   ; MAC paralelo com store
```

## Exemplo Completo: Transformar 4 Vértices por Matriz MVP

```asm
; SCU DSP: transform_batch
; Entrada (CT0 Data RAM):
;   [0..15]  = Matriz MVP (4×4 × int32, 16.16 fixed-point), carregada em CT0
;   [16..19] = Vértice 0 (x,y,z,w em int32)
;   [20..23] = Vértice 1
;   [24..27] = Vértice 2
;   [28..31] = Vértice 3
; Saída (CT2 Data RAM):
;   [0..3]  = Vértice 0 transformado (x,y,z,w)
;   [4..7]  = Vértice 1 transformado
;   etc.
;
; Nota: resultado de MAC(16.16, 16.16) = 32.32 no AC 48-bit
;       Precisamos pegar os bits [47:16] → são os bits [31:0] de AC >> 16

    ; Configurar pointers
    MVI #0,  CT0    ; CT0 aponta para início: matriz
    MVI #16, CT1    ; CT1 aponta para vértice 0
    MVI #0,  CT2    ; CT2 = destino resultado
    
    ; Loop sobre 4 vértices
    MVI #3, LOP     ; LOP = 3 (4 iterações: 0,1,2,3)
    LPS             ; Marcar início do loop

    ; ── Calcular X transformado ──────────────────────────────
    CLR A
    MVI #0, CT0     ; Resetar CT0 para linha 0 da matriz
    MAC CT0, CT1    ; AC += M[0] × V[x]
    MAC CT0, CT1    ; AC += M[1] × V[y]
    MAC CT0, CT1    ; AC += M[2] × V[z]
    MAC CT0, CT1    ; AC += M[3] × V[w]
    MOV A, CT2      ; CT2[out++] = AC (upper 32 bits = resultado 16.16 correto)

    ; ── Calcular Y transformado ──────────────────────────────
    CLR A
    MVI #4, CT0     ; CT0 aponta para linha 1 da matriz
    MAC CT0, CT1    ; AC += M[4] × V[x]  (CT1 foi adiantado, precisa resetar)
    ; ... (CT1 precisa ser resetado para o vértice atual antes de cada linha)
    ; Na prática: carregar CT1 com o endereço do vértice atual
    MOV A, CT2

    ; ── Calcular Z e W da mesma forma ───────────────────────
    ; ... (repetir para linhas 2 e 3 da matriz)

    LE              ; Loop end — decrementa LOP, volta para LPS se LOP > 0
    ENDI            ; Fim sem interrupção
```

> **Nota prática:** A maior dificuldade do SCU DSP é que CT1 (pointer para o vértice) avança automaticamente com cada MAC. Para transformar Y (linha 2 da matriz), CT1 já estará no vértice *seguinte*. A solução é ou: (a) duplicar os dados do vértice 4× na Data RAM, ou (b) usar MVI para resetar CT1 antes de cada linha (custa 1 ciclo por reset).

## Carregar Programa DSP do SH2

```cpp
// Copiar programa compilado (.dsp assembly → palavras binárias) para DSP Program RAM
void dsp_load_program(const uint32_t* prog, uint32_t word_count) {
    auto* dst = reinterpret_cast<volatile uint32_t*>(0x05FF8000);
    for (uint32_t i = 0; i < word_count; ++i) dst[i] = prog[i];
}

// Carregar dados para Data RAM CT0 via SCU DMA Ch2
void dsp_load_data_ct0(const uint32_t* data, uint32_t word_count) {
    // ATENÇÃO: 'data' deve estar em WRAM-L (0x002xxxxx)
    volatile uint32_t* D2R  = reinterpret_cast<volatile uint32_t*>(0x05A00040);
    volatile uint32_t* D2W  = reinterpret_cast<volatile uint32_t*>(0x05A00044);
    volatile uint32_t* D2C  = reinterpret_cast<volatile uint32_t*>(0x05A00048);
    volatile uint32_t* D2EN = reinterpret_cast<volatile uint32_t*>(0x05A00050);

    *D2R  = reinterpret_cast<uint32_t>(data);
    *D2W  = 0x05FF8400;           // CT0 Data RAM
    *D2C  = word_count * 4;
    *D2EN = 0x01000001;           // Iniciar DMA
    while (*D2EN & 0x01000000);   // Aguardar
}

// Executar DSP e aguardar
void dsp_run_sync() {
    volatile uint32_t* PPAF = reinterpret_cast<volatile uint32_t*>(0x05A00000);
    *PPAF = 0x00000101;           // Execute from PC=0
    while (*PPAF & 0x00010000);   // Wait for EXEC bit to clear
}
```

## Assemblador DSP

O Hitachi DSP assembler (`dspasm`) faz parte do SBL/SGL toolkit oficial. Alternativa:
- **SNASM DSP** (Psygnosis, encontrado no Saturn doc archive de antime.kapsi.fi)
- Escrever manualmente as palavras de 32-bit conforme o encoding do manual SCU (SCU User's Manual, Third Version)

## Errata Conhecida

1. **Documentação de opcode desatualizada** em algumas versões do manual. Cruzar com output do disassembler do Yabause/Kronos.
2. **Bus contention:** Se ambos SH2s estiverem ativos no B-bus enquanto DSP roda, pode ocorrer stall ou crash. Coordenar: desativar slave SH2 ou pausar durante execução DSP crítica.
3. **AC truncation:** MOV A armazena os 32 bits superiores do AC de 48 bits. Para 16.16 × 16.16 o resultado já está correto nessa posição (inteiro nos bits 31:16, fração nos 15:0).
4. **Ring buffer wrap silencioso:** CT pointer wrapa em 64 entries sem aviso. Dados além da posição 63 corrompem silenciosamente.
