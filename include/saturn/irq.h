#ifndef SATURN_IRQ_H
#define SATURN_IRQ_H

#include <stdint.h>
#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* SCU interrupts on the Master SH-2 (SCU manual, table 2.1). sat_init starts
 * them with VBlank-IN, which then drives sat_wait_vblank and keeps
 * sat_time_ms exact however long the program goes without calling it. A
 * host that never delivers VBlank-IN leaves the library polling, as before:
 * sat_irq_active() tells which. */
typedef enum sat_irq_source {
    SAT_IRQ_VBLANK_IN = 0,     /* level 15: display ends */
    SAT_IRQ_VBLANK_OUT = 1,    /* level 14: display starts; clears timer 0 */
    SAT_IRQ_HBLANK_IN = 2,     /* level 13: every line */
    SAT_IRQ_TIMER0 = 3,        /* level 12: the line set by sat_irq_set_timer0_line */
    SAT_IRQ_TIMER1 = 4,        /* level 11: dots into a line, sat_irq_set_timer1 */
    SAT_IRQ_DSP_END = 5,       /* level 10: SCU DSP ENDI */
    SAT_IRQ_SOUND_REQUEST = 6, /* level 9: SCSP */
    SAT_IRQ_SMPC = 7,          /* level 8: SMPC command done */
    SAT_IRQ_PAD = 8,           /* level 8 */
    SAT_IRQ_DMA2_END = 9,      /* level 6 */
    SAT_IRQ_DMA1_END = 10,     /* level 6 */
    SAT_IRQ_DMA0_END = 11,     /* level 5 */
    SAT_IRQ_DMA_ILLEGAL = 12,  /* level 3 */
    SAT_IRQ_SPRITE_END = 13,   /* level 2: VDP1 finished drawing */
    SAT_IRQ_SOURCE_COUNT = 14
} sat_irq_source_t;

/* Runs in interrupt context at the source's level: keep it short, never
 * wait for a frame, draw or call back into the library's frame functions.
 * Only higher-level sources can interrupt it. */
typedef void (*sat_irq_handler_t)(void* user);

/* 1 when SCU interrupts run (VBlank-IN arrived after sat_init), else 0. */
int sat_irq_active(void);

/* Installs a handler and unmasks its source; NULL removes it and masks the
 * source again. A VBlank-IN handler runs after the library's own frame
 * work, and VBlank-IN itself stays enabled. SAT_ERR_UNSUPPORTED while
 * interrupts are not active. */
sat_result_t sat_irq_set_handler(sat_irq_source_t source,
                                 sat_irq_handler_t handler, void* user);

/* Times the source was taken since sat_init. A source with no handler is
 * masked, so its count stays put; VBlank-IN always counts. */
uint32_t sat_irq_count(sat_irq_source_t source);

/* Timer 0 counts HBlank-IN from VBlank-OUT and fires when it equals
 * `line`: 1 is the line before the first displayed one, 0 fires with
 * VBlank-OUT (SCU manual 3.4). NTSC has 263 lines, so 0..263. */
sat_result_t sat_irq_set_timer0_line(uint16_t line);

/* Timer 1 reloads `dots` (1..511, about 7 MHz) at every HBlank-IN and fires
 * when it reaches 0: on every line, or only on the timer 0 line when
 * `timer0_line_only`. A count longer than a line (0x1AA dots at 320 wide)
 * never fires on consecutive lines. */
sat_result_t sat_irq_set_timer1(uint16_t dots, uint8_t timer0_line_only);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_IRQ_H */
