#ifndef SATURN_VIDEO_H
#define SATURN_VIDEO_H

#include <stdint.h>

#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Frame lifecycle                                                     */
/* ------------------------------------------------------------------ */
sat_result_t sat_begin_frame(void);
sat_result_t sat_end_frame(void);
sat_result_t sat_wait_vblank(void);

/* ------------------------------------------------------------------ */
/* Clear / backdrop color                                              */
/* ------------------------------------------------------------------ */

/* Define a cor de erase do VDP1. O erase limpa o framebuffer de sprites
 * no início de cada frame. O erase usa cor TRANSPARENTE (end code) para
 * permitir que o backdrop do VDP2 apareça nas áreas sem sprites.
 * O bit 15 (transparência) é adicionado automaticamente.
 */
sat_result_t sat_set_clear_color(uint16_t rgb555);

/* Define a cor de fundo do VDP2 (backdrop). Esta cor aparece quando
 * nenhuma camada VDP1/VDP2 desenha por cima. Com o erase transparente
 * do VDP1 (padrão), esta é a cor visível na tela.
 */
sat_result_t sat_vdp2_back_color_set(uint16_t rgb555);

/* Habilita ou desabilita o erase automático do VDP1 no frame inteiro.
 * enable=1: VDP1 faz erase com cor transparente (backdrop visível)
 * enable=0: VDP1 NÃO faz erase (framebuffer mantém dados do frame anterior)
 * Padrão: habilitado com cor transparente
 */
sat_result_t sat_vdp1_set_erase_enabled(uint8_t enable);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_VIDEO_H */
