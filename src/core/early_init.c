/* ===========================================================================
 * early_init.c - Saturn Hardware Early Initialization
 * ===========================================================================
 *
 * Esta função é chamada pelo sat_init() ANTES de qualquer configuração
 * específica do engine. Ela prepara o hardware para um estado conhecido.
 *
 * Por que é necessária:
 *   - O BIOS do Saturn NÃO inicializa VDP1/VDP2 completamente
 *   - Sem essa init, o hardware pode estar em estado indefinido
 *   - O libyaul faz isso em ___sys_init(), JoEngine em jo_core_init()
 *
 * Endereços de hardware (todos via região uncached 0x20000000):
 *   - VDP2 regs:    0x25F80000 (0x20000000 | 0x05F80000)
 *   - VDP1 regs:    0x25D00000 (0x20000000 | 0x05D00000)
 *   - VDP1 VRAM:    0x25C00000 (0x20000000 | 0x05C00000)
 *   - VDP2 VRAM:    0x25E00000 (0x20000000 | 0x05E00000)
 *   - SCU regs:     0x20C00000
 *   - SMPC regs:    0x20100000
 *
 * Por que usar endereços uncached (0x2x...):
 *   - Acesso cached (0x0x...) pode causar problemas com hardware
 *   - A região uncached garante que writes cheguem imediatamente
 *   - VDP1/VDP2 registram precisam de acesso uncached para funcionar
 *
 * Nota sobre índices de array:
 *   Ponteiros são `unsigned short*` (16 bits), então cada índice = 2 bytes.
 *   Exemplo: vdp2[7] = offset 0x0E (14 bytes = índice 7 * 2)
 * =========================================================================== */

__attribute__((used))
void _saturn_early_init(void) {
    /* Ponteiros para registros de hardware (região uncached) */
    volatile unsigned short* const vdp2 = (volatile unsigned short*)0x25F80000u;
    volatile unsigned short* const vdp1 = (volatile unsigned short*)0x25D00000u;
    volatile unsigned int* const vdp1_vram = (volatile unsigned int*)0x25C00000u;
    volatile unsigned short* const scu = (volatile unsigned short*)0x20C00000u;
    int i;

    /* =========================================================================
     * 1. SCU (System Control Unit) - Controlador de interrupções e DMA
     * =========================================================================
     * Offset 0x00 (índice 0): ICU - Interrupt Control Unit
     * Offset 0x04 (índice 2): IMR - Interrupt Mask Register
     * ========================================================================= */
    scu[0] = 0;   /* ICU: limpar pedidos de interrupção pendentes */
    scu[2] = 0;   /* IMR: mascarar todas as interrupções */

    /* =========================================================================
     * 2. VDP2 (Video Display Processor 2) - Backgrounds e scroll
     * =========================================================================
     * Passo 1: Desabilitar display para evitar glitches durante config
     * ========================================================================= */
    vdp2[0] = 0x0000;   /* TVMD (offset 0x00): Display OFF
                         *   Bit 15 (DISP) = 0: desabilitar output de vídeo
                         *   Bit 8 (BDCLMD) = 0: backdrop color mode */

    /* Pequeno delay para garantir que o VDP2 processou o comando */
    for (i = 0; i < 1000; i++) { }

    /* =========================================================================
     * Passo 2: Configurar VRAM e ciclo de barramento
     * ========================================================================= */
    vdp2[7] = 0x1100;   /* RAMCTL (offset 0x0E, índice 7):
                         *   Bits 15-14: VRAM mode (01 = 512K mode)
                         *   Bit 12: 1 = VRAM A/B dual port
                         *   Bit 8: 1 = VRAM access priority */

    /* Cycle patterns: controlam acesso à VRAM entre CPU, VDP1, VDP2, DMA
     * Cada nibble (4 bits) = prioridade de acesso para um slot de clock
     * 0xE = CPU tem acesso, outros dispositivos bloqueados
     * 0x4 = VDP2 tem acesso para leitura
     * Configuração safe: CPU dominante durante init */
    for (i = 0; i < 8; i++) {
        vdp2[8 + i] = 0xEEEE;   /* CYCA0L-U, CYCA1L-U, CYCB0L-U, CYCB1L-U
                                 * Offsets 0x10-0x1E (índices 8-15) */
    }

    /* =========================================================================
     * Passo 3: Desabilitar todos os backgrounds
     * ========================================================================= */
    vdp2[16] = 0x0000;  /* BGON (offset 0x20, índice 16):
                         *   Bits 0-7: habilitar NBG0-3, RBG0
                         *   0 = todos desabilitados */

    /* =========================================================================
     * Passo 4: Zerar scroll e zoom
     * ========================================================================= */
    vdp2[56] = 0;       /* SCXIN0 (offset 0x70, índice 56): Scroll X integer NBG0 */
    vdp2[57] = 0;       /* SCXDN0 (offset 0x72, índice 57): Scroll X fraction NBG0 */
    vdp2[58] = 0;       /* SCYIN0 (offset 0x74, índice 58): Scroll Y integer NBG0 */
    vdp2[59] = 0;       /* SCYDN0 (offset 0x76, índice 59): Scroll Y fraction NBG0 */

    /* =========================================================================
     * Passo 5: Configurar prioridades de layer
     * ========================================================================= */
    vdp2[120] = 0x0606; /* PRISA (offset 0xF0, índice 120):
                         *   Prioridade: NBG3 > NBG2 > NBG1 > NBG0
                         *   0x0606 = prioridade padrão */
    vdp2[124] = 0x0001; /* PRINA (offset 0xF8, índice 124):
                         *   Prioridade: CC (color calculation) > Sprites > Backdrop */

    /* Delay para estabilizar configuração */
    for (i = 0; i < 1000; i++) { }

    /* =========================================================================
     * Passo 6: Habilitar display com backdrop color
     * ========================================================================= */
    vdp2[0] = 0x8100;   /* TVMD (offset 0x00):
                         *   Bit 15 (DISP) = 1: habilitar output de vídeo
                         *   Bit 8 (BDCLMD) = 1: backdrop color enable
                         *   Cor do backdrop definida na tabela em VRAM */

    /* Delay para VDP2 processar */
    for (i = 0; i < 1000; i++) { }

    /* =========================================================================
     * 3. VDP1 (Video Display Processor 1) - Sprites e polígonos
     * =========================================================================
     * Desabilitar e limpar VDP1 para evitar comandos pendentes
     * ========================================================================= */
    vdp1[0] = 0x0000;   /* TVMR (offset 0x00): TV Mode
                         *   Bit 15 = 0: VDP1 disable
                         *   Bit 3 = 0: não resetar */
    vdp1[1] = 0x0000;   /* FBCR (offset 0x02): Frame Buffer Control */
    vdp1[2] = 0x0000;   /* PTMR (offset 0x04): Process Timer
                         *   0 = parar processamento de comandos */
    vdp1[3] = 0x0000;   /* EWDR (offset 0x06): End Write Data */
    vdp1[4] = 0x0000;   /* EWLR (offset 0x08): End Write Left */
    vdp1[5] = 0x0000;   /* EWRR (offset 0x0A): End Write Right */

    /* =========================================================================
     * 4. Marcar fim de lista de comandos na VDP1 VRAM
     * =========================================================================
     * Comando "end" (0x80000000) indica fim da lista de comandos.
     * VDP1 para de processar quando encontra este comando.
     * ========================================================================= */
    vdp1_vram[0] = 0x80000000u;  /* End-of-command marker
                                   *   Bit 31 = 1: último comando
                                   *   Bits 30-28 = 0: comando NOP */
}
