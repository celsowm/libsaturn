/* ===========================================================================
 * crt0.s - Sega Saturn Boot Entry Point
 * ===========================================================================
 *
 * Fluxo de boot do Saturn:
 *   1. BIOS lê IP.BIN do CD (setor 0)
 *   2. BIOS carrega 0.BIN (nosso executável) em 0x06004000
 *   3. BIOS salta para _start (entry point definido no linker)
 *   4. _start: desabilita IRQs, configura stack, limpa BSS
 *   5. Chama _saturn_early_init() para inicializar hardware
 *   6. Chama _main() (função main do usuário)
 *
 * Diferença vs libyaul:
 *   - libyaul: _start -> ___sys_init() -> main()
 *   - libsaturn: _start -> _saturn_early_init() -> _main()
 *   - JoEngine: usa -nostartfiles, main() direto como entry point
 *
 * Endereços de memória:
 *   - Work RAM High (WRAMH): 0x06000000 - 0x060FFFFF (1MB, código + dados)
 *   - Work RAM Low (WRAML): 0x00200000 - 0x002FFFFF (512KB, dados)
 *   - Stack: topo em 0x060FFFFC (fim da WRAMH)
 * =========================================================================== */

    .section .text.start
    .global _start
    .align 4

_start:
    /* -------------------------------------------------------------------
     * 1. Desabilitar interrupções e configurar modo CPU
     * -------------------------------------------------------------------
     * SR (Status Register) bits de máscara de interrupção:
     *   I3-I0 (bits 4-7) = nível de máscara de IRQ
     *   0xF0 = mascarar todas as IRQs (modo mais restritivo)
     *   BL (bit 0) = branch delay slot
     * ------------------------------------------------------------------- */
    stc     sr, r0
    mov.l   .Lsr_mask, r1
    or      r1, r0
    ldc     r0, sr

    /* -------------------------------------------------------------------
     * 2. Configurar stack pointer (r15)
     * -------------------------------------------------------------------
     * Stack cresce para baixo em SH2.
     * Topo = 0x060FFFFC (último endereço alinhado de WRAMH)
     * ------------------------------------------------------------------- */
    mov.l   .Lstack_top, r15

    /* -------------------------------------------------------------------
     * 3. Zerar seção BSS (variáveis globais não inicializadas)
     * -------------------------------------------------------------------
     * __bss_start e __bss_end definidos pelo linker (saturn.ld)
     * ------------------------------------------------------------------- */
    xor     r2, r2              /* r2 = 0 (valor para zerar) */
    mov.l   .Lbss_start, r0     /* r0 = endereço inicial BSS */
    mov.l   .Lbss_end, r1       /* r1 = endereço final BSS */
    cmp/eq  r0, r1              /* BSS vazio? */
    bt      .Lmain_call         /* Sim -> pular zeroing */
.Lloop:
    mov.l   r2, @r0             /* Escreve 0 na posição atual */
    add     #4, r0              /* Avança 4 bytes */
    cmp/hs  r1, r0              /* Chegou ao fim? */
    bf      .Lloop              /* Não -> continua */

    /* -------------------------------------------------------------------
     * 4. Chamar main()
     * -------------------------------------------------------------------
     * O early_init é chamado dentro de sat_init(), não aqui.
     * Isso permite que o main() do usuário execute primeiro se quiser.
     * ------------------------------------------------------------------- */
.Lmain_call:
    mov.l   .Lmain, r0
    jsr     @r0
    nop

    /* Loop infinito se main() retornar (não deveria) */
hang:
    bra     hang
    nop

    /* -------------------------------------------------------------------
     * Tabela de literais (acessados via mov.l PC-relative)
     * ------------------------------------------------------------------- */
    .align 4
.Lsr_mask:   .long 0x000000F0   /* Máscara: desabilitar todas as IRQs */
.Lstack_top: .long 0x060FFFFC   /* Topo da stack (fim da WRAMH) */
.Lbss_start: .long __bss_start   /* Início da seção BSS (linker symbol) */
.Lbss_end:   .long __bss_end     /* Fim da seção BSS (linker symbol) */
.Lmain:      .long _main         /* Endereço da função main() */
