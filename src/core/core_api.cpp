#include "saturn/core.h"

#include "src/core/internal.hpp"
#include "src/core/runtime_state.hpp"
#include "src/hal/scu.hpp"
#include "src/hal/vdp1.hpp"
#include "src/hal/vdp2.hpp"

extern "C" sat_result_t sat_init(const sat_video_config_t* config) {
    using namespace saturn;
    using namespace saturn::core;

    /* -------------------------------------------------------------------
     * Early hardware initialization
     * -------------------------------------------------------------------
     * PROBLEMA: O BIOS do Saturn NÃO inicializa VDP1/VDP2 completamente.
     * Se chamarmos saturn::hal::vdp2::init_ntsc_320x224() direto, o hardware pode
     * estar em estado indefinido causando glitch visual ou crash.
     *
     * SOLUÇÃO: _saturn_early_init() prepara o hardware para um estado
     * conhecido antes de qualquer configuração específica do engine.
     *
     * Comparação com outros engines:
     *   - libyaul: faz isso em ___sys_init() (chamado pelo crt0)
     *   - JoEngine: faz isso em jo_core_init() (chamado pelo main)
     *   - libsaturn: faz isso aqui, no início de sat_init()
     * ------------------------------------------------------------------- */
    extern void _saturn_early_init(void);
    _saturn_early_init();

    /* Validar parâmetros */
    if (config == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    if (config->width != saturn::internal::kDefaultWidth || config->height != saturn::internal::kDefaultHeight) {
        return SAT_ERR_UNSUPPORTED;
    }
    if (config->ntsc == 0u) {
        return SAT_ERR_UNSUPPORTED;
    }

    /* Inicializar estado do runtime */
    g_state.config = *config;
    g_state.pad = {0, 0, 0};
    g_state.clear_color = 0x0000;
    g_state.nbg0_map_plane_index = 0x0010u;
    g_state.nbg0_map_width = 64u;
    g_state.nbg0_map_height = 64u;
    g_state.initialized = true;

    /* -------------------------------------------------------------------
     * Inicialização completa do hardware
     * -------------------------------------------------------------------
     * Agora que o hardware está em estado limpo (_saturn_early_init),
     * configuramos cada subsistema com os parâmetros desejados.
     *
     * Ordem de init:
     *   1. VDP2 (backgrounds, scroll, VRAM)
     *   2. VDP1 (sprites, polígonos, frame buffer)
     *   3. SCU (interrupts, VBlank counter)
     *   4. VDP1 begin_frame (preparar command buffer)
     * ------------------------------------------------------------------- */
    saturn::hal::vdp2::init_ntsc_320x224();
    saturn::hal::vdp1::init(config->width, config->height, g_state.clear_color);
    saturn::hal::scu::init_interrupts();
    saturn::hal::vdp1::begin_frame(g_state.command_buffer, saturn::internal::kCmdCapacity);
    return SAT_OK;
}

extern "C" sat_result_t sat_shutdown(void) {
    using namespace saturn::core;
    g_state.initialized = false;
    return SAT_OK;
}
