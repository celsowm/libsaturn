#include "saturn/vdp2_layers.h"

#include "saturn/math3d.h"

#include "src/core/runtime/state.hpp"
#include "src/hal/vdp2/vdp2.hpp"

namespace {

namespace hal = saturn::hal::vdp2;
namespace nbg = saturn::hal::vdp2::nbg;

constexpr uint32_t kZoomMin = 0x4000u;      /* 1/4 */
constexpr uint32_t kZoomMax = 0x80000u;     /* 8x */

sat_result_t to_result(hal::NbgResult r) {
    switch (r) {
        case hal::NbgResult::Ok: return SAT_OK;
        case hal::NbgResult::Busy: return SAT_ERR_BUSY;
        case hal::NbgResult::NoCyclePattern: return SAT_ERR_CAPACITY;
        default: return SAT_ERR_INVALID_ARG;
    }
}

/* Coordinate increment (16.16) for a magnification (16.16). */
uint32_t increment_for(uint32_t zoom) {
    return static_cast<uint32_t>((1ull << 32) / zoom);
}

uint8_t reduction_for(uint32_t x_increment) {
    if (x_increment <= 0x10000u) return 0u;
    if (x_increment <= 0x20000u) return 1u;
    return 2u;
}

bool valid_layer(sat_vdp2_layer_t layer) {
    return static_cast<uint32_t>(layer) < nbg::kLayerCount;
}

}  // namespace

extern "C" void sat_vdp2_layer_config_default(sat_vdp2_layer_t layer,
                                              sat_vdp2_layer_config_t* out_config) {
    if (out_config == nullptr) return;
    *out_config = {};
    out_config->layer = static_cast<uint8_t>(layer);
    out_config->color_mode = SAT_VDP2_COLOR_MODE_16;
    out_config->char_size = SAT_VDP2_CHAR_SIZE_1X1;
    out_config->pattern_name_words = 1u;
    out_config->plane_pages_x = 1u;
    out_config->plane_pages_y = 1u;
    out_config->priority = 1u;
    out_config->transparent = 1u;
}

extern "C" sat_result_t sat_vdp2_layer_configure(const sat_vdp2_layer_config_t* config) {
    SAT_TRY(saturn::core::require_initialized());
    if (config == nullptr || config->layer >= nbg::kLayerCount || config->color_mode > 4u ||
        config->char_size > 1u || config->priority > 7u) {
        return SAT_ERR_INVALID_ARG;
    }
    nbg::Layer layer{};
    layer.enabled = true;
    layer.bitmap = config->bitmap != 0u;
    layer.colors = static_cast<nbg::Colors>(config->color_mode);
    layer.char_2x2 = config->char_size == SAT_VDP2_CHAR_SIZE_2X2;
    layer.pn_one_word = config->pattern_name_words != 2u;
    if (config->pattern_name_words != 1u && config->pattern_name_words != 2u) return SAT_ERR_INVALID_ARG;
    layer.pages_x = config->plane_pages_x;
    layer.pages_y = config->plane_pages_y;
    layer.bitmap_size = config->bitmap_size;
    layer.priority = config->priority;
    layer.transparent = config->transparent != 0u;
    layer.palette = static_cast<uint8_t>(config->palette & 7u);
    layer.palette_supp = static_cast<uint8_t>(config->palette & 7u);
    layer.char_banks = static_cast<uint8_t>(config->char_bank_mask & 0x0Fu);
    for (uint8_t p = 0u; p < 4u; ++p) layer.plane_address[p] = config->plane_address[p];
    layer.vcs = config->vertical_cell_scroll != 0u;
    layer.vcs_address = config->vertical_cell_scroll_address;
    if (layer.vcs && (layer.vcs_address & 3u) != 0u) return SAT_ERR_INVALID_ARG;
    if (!layer.bitmap && layer.pn_one_word) {
        /* The 10-bit character numbers of 1-word names index a window; the
         * five auxiliary bits of the pattern name control register place it. */
        const uint32_t window = layer.char_2x2 ? 0x20000u : 0x8000u;
        if (config->char_base_address % window != 0u || config->char_base_address >= nbg::kVramBytes) {
            return SAT_ERR_INVALID_ARG;
        }
        const uint32_t units = config->char_base_address / window;
        layer.char_number_supp = static_cast<uint8_t>(layer.char_2x2 ? ((units & 7u) << 2u)
                                                                     : (units & 0x1Fu));
    } else if (!layer.bitmap && config->char_base_address != 0u) {
        return SAT_ERR_INVALID_ARG;   /* 2-word names carry the whole character number */
    }
    /* Mosaic on NBG0/NBG1 takes the vertical cell scroll away. */
    if (layer.vcs && hal::mosaic_blocks_vcs(config->layer)) return SAT_ERR_UNSUPPORTED;
    /* A layer already configured keeps its zoom and its line scroll. */
    nbg::Layer previous{};
    if (hal::nbg_layer(config->layer, &previous)) {
        layer.reduction = previous.reduction;
        layer.ls_h = previous.ls_h;
        layer.ls_v = previous.ls_v;
        layer.ls_zoom = previous.ls_zoom;
        layer.ls_interval = previous.ls_interval;
        layer.ls_address = previous.ls_address;
    }
    return to_result(hal::nbg_configure(config->layer, layer));
}

extern "C" sat_result_t sat_vdp2_layer_release(sat_vdp2_layer_t layer) {
    SAT_TRY(saturn::core::require_initialized());
    if (!valid_layer(layer)) return SAT_ERR_INVALID_ARG;
    nbg::Layer current{};
    if (!hal::nbg_layer(static_cast<uint8_t>(layer), &current)) return SAT_ERR_NOT_FOUND;
    hal::nbg_release(static_cast<uint8_t>(layer));
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_layer_set_enabled(sat_vdp2_layer_t layer, uint8_t enabled) {
    SAT_TRY(saturn::core::require_initialized());
    if (!valid_layer(layer)) return SAT_ERR_INVALID_ARG;
    nbg::Layer current{};
    if (!hal::nbg_layer(static_cast<uint8_t>(layer), &current)) return SAT_ERR_NOT_FOUND;
    current.enabled = enabled != 0u;
    return to_result(hal::nbg_configure(static_cast<uint8_t>(layer), current));
}

extern "C" sat_result_t sat_vdp2_layer_set_priority(sat_vdp2_layer_t layer, uint8_t priority) {
    SAT_TRY(saturn::core::require_initialized());
    if (!valid_layer(layer) || priority > 7u) return SAT_ERR_INVALID_ARG;
    nbg::Layer current{};
    if (!hal::nbg_layer(static_cast<uint8_t>(layer), &current)) return SAT_ERR_NOT_FOUND;
    current.priority = priority;
    return to_result(hal::nbg_configure(static_cast<uint8_t>(layer), current));
}

extern "C" sat_result_t sat_vdp2_layer_set_scroll(sat_vdp2_layer_t layer, int32_t x, int32_t y) {
    SAT_TRY(saturn::core::require_initialized());
    if (!valid_layer(layer)) return SAT_ERR_INVALID_ARG;
    nbg::Layer current{};
    if (!hal::nbg_layer(static_cast<uint8_t>(layer), &current)) return SAT_ERR_NOT_FOUND;
    /* Integer part 11 bits (the screen repeats past the display area), and the
     * fraction's top 8 bits for NBG0/NBG1. */
    const uint16_t xi = static_cast<uint16_t>((x >> 16) & 0x07FF);
    const uint16_t yi = static_cast<uint16_t>((y >> 16) & 0x07FF);
    const uint16_t xf = static_cast<uint16_t>(((x >> 8) & 0xFF) << 8u);
    const uint16_t yf = static_cast<uint16_t>(((y >> 8) & 0xFF) << 8u);
    hal::nbg_set_scroll(static_cast<uint8_t>(layer), xi, xf, yi, yf);
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_layer_set_zoom(sat_vdp2_layer_t layer, uint32_t zoom_x,
                                                uint32_t zoom_y) {
    SAT_TRY(saturn::core::require_initialized());
    if (static_cast<uint32_t>(layer) > 1u) return SAT_ERR_INVALID_ARG;
    if (zoom_x < kZoomMin || zoom_x > kZoomMax || zoom_y < kZoomMin || zoom_y > kZoomMax) {
        return SAT_ERR_INVALID_ARG;
    }
    nbg::Layer current{};
    if (!hal::nbg_layer(static_cast<uint8_t>(layer), &current)) return SAT_ERR_NOT_FOUND;
    const uint32_t xi = increment_for(zoom_x);
    const uint32_t yi = increment_for(zoom_y);
    const uint8_t reduction = reduction_for(xi);
    if (reduction != current.reduction) {
        current.reduction = reduction;
        SAT_TRY(to_result(hal::nbg_configure(static_cast<uint8_t>(layer), current)));
    }
    hal::nbg_set_zoom_increment(static_cast<uint8_t>(layer),
                                static_cast<uint16_t>((xi >> 16) & 0x7u),
                                static_cast<uint16_t>(((xi >> 8) & 0xFFu) << 8u),
                                static_cast<uint16_t>((yi >> 16) & 0x7u),
                                static_cast<uint16_t>(((yi >> 8) & 0xFFu) << 8u));
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_layer_cycle_patterns(uint16_t out_registers[8]) {
    SAT_TRY(saturn::core::require_initialized());
    if (out_registers == nullptr) return SAT_ERR_INVALID_ARG;
    for (uint8_t i = 0u; i < 8u; ++i) out_registers[i] = hal::nbg_cycle_word(i);
    return SAT_OK;
}

/* ------------------------------------------------------------------ */
/* Raster effects                                                      */
/* ------------------------------------------------------------------ */

namespace {

constexpr uint32_t kChunkWords = 64u;

/* Writes `words` at a byte address through the validated VRAM writer. */
sat_result_t write_table(uint32_t byte_address, const uint16_t* words, uint32_t count) {
    return sat_vdp2_vram_write_words(byte_address / 2u, words, count);
}

uint32_t entry_words(const nbg::Layer& l) {
    return nbg::line_scroll_entry_words(l.ls_h, l.ls_v, l.ls_zoom);
}

/* Appends one entry's words for the layer's enabled fields. */
uint32_t encode_entry(const nbg::Layer& l, const sat_vdp2_line_scroll_entry_t& e, uint16_t* out) {
    uint32_t n = 0u;
    if (l.ls_h) { nbg::encode_scroll(e.x, &out[n], &out[n + 1u]); n += 2u; }
    if (l.ls_v) { nbg::encode_scroll(e.y, &out[n], &out[n + 1u]); n += 2u; }
    if (l.ls_zoom) { nbg::encode_increment(e.zoom, &out[n], &out[n + 1u]); n += 2u; }
    return n;
}

}  // namespace

/* ------------------------------------------------------------------ */
/* RBG0, cell format                                                    */
/* ------------------------------------------------------------------ */

namespace rot = saturn::hal::vdp2::rot;

extern "C" void sat_vdp2_rotation_config_default(sat_vdp2_rotation_config_t* out_config) {
    if (out_config == nullptr) return;
    *out_config = {};
    out_config->color_mode = SAT_VDP2_COLOR_MODE_256;
    out_config->char_size = SAT_VDP2_CHAR_SIZE_1X1;
    out_config->pattern_name_words = 1u;
    out_config->plane_pages_x = 1u;
    out_config->plane_pages_y = 1u;
    out_config->priority = 1u;
    out_config->transparent = 1u;
    out_config->over = SAT_VDP2_ROTATION_OVER_REPEAT;
    out_config->parameter_mode = SAT_VDP2_ROTATION_PARAMETER_A;
    /* parameter-set-B's plane map (RBG1's names live here when enabled). */
    out_config->map_b.plane_pages_x = 1u;
    out_config->map_b.plane_pages_y = 1u;
    out_config->map_b.over = SAT_VDP2_ROTATION_OVER_REPEAT;
    /* RBG1 defaults: hidden, 256-colour 1x1 1-word names, priority 1,
     * transparent, character window base = 0 (caller sets it). */
    out_config->rbg1.color_mode = SAT_VDP2_COLOR_MODE_256;
    out_config->rbg1.char_size = SAT_VDP2_CHAR_SIZE_1X1;
    out_config->rbg1.pattern_name_words = 1u;
    out_config->rbg1.priority = 1u;
    out_config->rbg1.transparent = 1u;
}

extern "C" sat_result_t sat_vdp2_rotation_configure(const sat_vdp2_rotation_config_t* config) {
    SAT_TRY(saturn::core::require_initialized());
    if (config == nullptr || config->color_mode > 3u || config->char_size > 1u ||
        (config->pattern_name_words != 1u && config->pattern_name_words != 2u)) {
        return SAT_ERR_INVALID_ARG;
    }
    rot::Layer l{};
    l.enabled = true;
    l.colors = static_cast<nbg::Colors>(config->color_mode);
    l.char_2x2 = config->char_size == SAT_VDP2_CHAR_SIZE_2X2;
    l.pn_one_word = config->pattern_name_words == 1u;
    l.pages_x = config->plane_pages_x;
    l.pages_y = config->plane_pages_y;
    l.priority = config->priority;
    l.transparent = config->transparent != 0u;
    l.over = config->over;
    l.over_pattern_name = config->over_pattern_name;
    l.palette_supp = static_cast<uint8_t>(config->palette & 7u);
    l.name_banks = static_cast<uint8_t>(config->name_bank_mask);
    l.char_banks = static_cast<uint8_t>(config->char_bank_mask);
    for (uint8_t p = 0u; p < rot::kPlanes; ++p) l.plane_address[p] = config->plane_address[p];
    l.table_address = config->table_address;
    l.parameter_mode = config->parameter_mode;
    l.coefficients_a.enabled = config->coefficients_a.enabled != 0u;
    l.coefficients_a.mode = static_cast<uint8_t>(config->coefficients_a.mode & 3u);
    l.coefficients_a.one_word = config->coefficients_a.one_word != 0u;
    l.coefficients_b.enabled = config->coefficients_b.enabled != 0u;
    l.coefficients_b.mode = static_cast<uint8_t>(config->coefficients_b.mode & 3u);
    l.coefficients_b.one_word = config->coefficients_b.one_word != 0u;
    if (l.pn_one_word) {
        /* as for the NBGs: the 10-bit character numbers index a window */
        const uint32_t window = l.char_2x2 ? 0x20000u : 0x8000u;
        if (config->char_base_address % window != 0u || config->char_base_address >= nbg::kVramBytes) {
            return SAT_ERR_INVALID_ARG;
        }
        const uint32_t units = config->char_base_address / window;
        l.char_number_supp = static_cast<uint8_t>(l.char_2x2 ? ((units & 7u) << 2u) : (units & 0x1Fu));
    } else if (config->char_base_address != 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    /* Parameter-set-B map (B's 16 planes, own pages/over, plane_address[]). */
    l.map_b.pages_x = config->map_b.plane_pages_x;
    l.map_b.pages_y = config->map_b.plane_pages_y;
    l.map_b.over = static_cast<uint8_t>(config->map_b.over & 3u);
    l.map_b.over_pattern_name = config->map_b.over_pattern_name;
    for (uint8_t p = 0u; p < rot::kPlanes; ++p) l.map_b.plane_address[p] = config->map_b.plane_address[p];
    /* RBG1 sub-config: enabled drives the validation; the colour, char
     * size, name-word size, priority, transparent, palette are 1:1 fields;
     * the plane map is shared with parameter set B (cfg.map_b). */
    const sat_vdp2_rbg1_config_t* r = &config->rbg1;
    l.rbg1.enabled = r->enabled != 0u;
    l.rbg1.colors = static_cast<nbg::Colors>(r->color_mode & 3u);
    l.rbg1.char_2x2 = r->char_size == SAT_VDP2_CHAR_SIZE_2X2;
    l.rbg1.pn_one_word = r->pattern_name_words == 1u;
    l.rbg1.priority = r->priority;
    l.rbg1.transparent = r->transparent != 0u;
    l.rbg1.palette_supp = static_cast<uint8_t>(r->palette & 7u);
    l.rbg1.char_base_address = r->char_base_address;
    /* Character number supplementary for RBG1 follows the same 10-bit
     * window rule (p04_62): the 8-bit char_number_supp indexes one window. */
    if (l.rbg1.pn_one_word) {
        const uint32_t window = l.rbg1.char_2x2 ? 0x20000u : 0x8000u;
        if (r->char_base_address % window != 0u || r->char_base_address >= nbg::kVramBytes) {
            return SAT_ERR_INVALID_ARG;
        }
        const uint32_t units = r->char_base_address / window;
        l.rbg1.char_number_supp = static_cast<uint8_t>(l.rbg1.char_2x2 ? ((units & 7u) << 2u)
                                                                       : (units & 0x1Fu));
    } else if (r->char_base_address != 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    /* a region already in use stays (its coefficient offsets go with it) */
    rot::Layer previous{};
    if (hal::rot_layer(&previous) && previous.table_address == l.table_address) {
        l.ktaof = previous.ktaof;
        l.ktaof_b = previous.ktaof_b;
    }
    return to_result(hal::rot_configure(l));
}

extern "C" sat_result_t sat_vdp2_rotation_release(void) {
    SAT_TRY(saturn::core::require_initialized());
    rot::Layer current{};
    if (!hal::rot_layer(&current)) return SAT_ERR_NOT_FOUND;
    hal::rot_release();
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_rotation_set_enabled(uint8_t enabled) {
    SAT_TRY(saturn::core::require_initialized());
    rot::Layer current{};
    if (!hal::rot_layer(&current)) return SAT_ERR_NOT_FOUND;
    current.enabled = enabled != 0u;
    return to_result(hal::rot_configure(current));
}

extern "C" sat_result_t sat_vdp2_rotation_set_priority(uint8_t priority) {
    SAT_TRY(saturn::core::require_initialized());
    if (priority > 7u) return SAT_ERR_INVALID_ARG;
    rot::Layer current{};
    if (!hal::rot_layer(&current)) return SAT_ERR_NOT_FOUND;
    current.priority = priority;
    return to_result(hal::rot_configure(current));
}

extern "C" sat_result_t sat_vdp2_rotation_set_parameter_mode(uint8_t mode) {
    SAT_TRY(saturn::core::require_initialized());
    if (mode > SAT_VDP2_ROTATION_PARAMETER_COEFFICIENT) return SAT_ERR_INVALID_ARG;
    rot::Layer current{};
    if (!hal::rot_layer(&current)) return SAT_ERR_NOT_FOUND;
    current.parameter_mode = mode;
    return to_result(hal::rot_configure(current));
}

extern "C" sat_result_t sat_vdp2_rotation_rbg1_set_enabled(uint8_t enabled) {
    SAT_TRY(saturn::core::require_initialized());
    rot::Layer current{};
    if (!hal::rot_layer(&current)) return SAT_ERR_NOT_FOUND;
    if (!hal::rot_set_rbg1_enabled(enabled != 0u)) return SAT_ERR_INVALID_ARG;
    return SAT_OK;
}

/* One set's 17 values plus its coefficient start, in the policy's layout. */
static rot::Params to_rot_params(const sat_vdp2_rotation_params_t* in, bool one_word) {
    rot::Params p{};
    p.xst = in->xst;
    p.yst = in->yst;
    p.zst = in->zst;
    p.dxst = in->dxst;
    p.dyst = in->dyst;
    p.dx = in->dx;
    p.dy = in->dy;
    for (uint32_t i = 0u; i < 6u; ++i) p.m[i] = in->matrix[i];
    p.px = in->px;
    p.py = in->py;
    p.pz = in->pz;
    p.cx = in->cx;
    p.cy = in->cy;
    p.cz = in->cz;
    p.mx = in->mx;
    p.my = in->my;
    p.kx = in->kx;
    p.ky = in->ky;
    p.kast = (one_word ? rot::kast_integer_1word(in->coefficient_address)
                       : rot::kast_integer(in->coefficient_address)) << 16u;
    p.dkast = in->coefficient_step;
    p.dkax = 0;
    return p;
}

extern "C" sat_result_t sat_vdp2_rotation_write_params(uint32_t table_address,
                                                       const sat_vdp2_rotation_params_t* params_a,
                                                       const sat_vdp2_rotation_params_t* params_b) {
    SAT_TRY(saturn::core::require_initialized());
    if (params_a == nullptr || params_b == nullptr) return SAT_ERR_INVALID_ARG;
    /* The whole A+B region: 4-byte aligned, bit 7 clear (RPTA bit 6 is
     * ignored and fixed, p06_32), inside VRAM. */
    if ((table_address & 3u) != 0u || (table_address & 0x80u) != 0u ||
        table_address + rot::kParamsABBytes > nbg::kVramBytes) {
        return SAT_ERR_INVALID_ARG;
    }
    rot::Layer current{};
    if (!hal::rot_layer(&current)) return SAT_ERR_NOT_FOUND;
    current.table_address = table_address;
    if (rot::validate(current) != rot::Status::Ok) return SAT_ERR_INVALID_ARG;
    /* The entry alignment and the KTAOF offset unit follow each set's own
     * contract (p06_43): 2-word entries count 4 bytes, 1-word entries 2. */
    const sat_vdp2_rotation_params_t* sets[2] = {params_a, params_b};
    const rot::Coefficients* contracts[2] = {&current.coefficients_a, &current.coefficients_b};
    for (uint32_t s = 0u; s < 2u; ++s) {
        if (!contracts[s]->enabled) continue;
        const uint32_t mask = contracts[s]->one_word ? 1u : 3u;
        if ((sets[s]->coefficient_address & mask) != 0u ||
            sets[s]->coefficient_address >= nbg::kVramBytes) {
            return SAT_ERR_INVALID_ARG;
        }
    }
    const rot::Params pa = to_rot_params(params_a, current.coefficients_a.one_word);
    const rot::Params pb = to_rot_params(params_b, current.coefficients_b.one_word);
    uint16_t words[rot::kParamsABWords];
    rot::encode_params_ab(pa, pb, words);
    SAT_TRY(write_table(table_address, words, rot::kParamsABWords));
    const uint8_t ktaof_a = current.coefficients_a.enabled
        ? (current.coefficients_a.one_word ? rot::ktaof_for_1word(params_a->coefficient_address)
                                           : rot::ktaof_for(params_a->coefficient_address))
        : current.ktaof;
    const uint8_t ktaof_b = current.coefficients_b.enabled
        ? (current.coefficients_b.one_word ? rot::ktaof_for_1word(params_b->coefficient_address)
                                           : rot::ktaof_for(params_b->coefficient_address))
        : current.ktaof_b;
    hal::rot_set_table(table_address, ktaof_a, ktaof_b);
    return SAT_OK;
}

extern "C" uint32_t sat_vdp2_rotation_coefficient_2word(int32_t value, uint8_t flags) {
    const bool flagged = (flags & (SAT_VDP2_COEFFICIENT_HIDE | SAT_VDP2_COEFFICIENT_USE_B)) != 0u;
    return rot::encode_coefficient(value, flagged);
}

extern "C" uint16_t sat_vdp2_rotation_coefficient_1word(int32_t value, uint8_t flags) {
    const bool flagged = (flags & (SAT_VDP2_COEFFICIENT_HIDE | SAT_VDP2_COEFFICIENT_USE_B)) != 0u;
    return rot::encode_coefficient_1word(value, flagged);
}

extern "C" sat_result_t sat_vdp2_rotation_write_coefficients(uint32_t byte_address, const uint32_t* coefficients,
                                                             uint32_t count) {
    SAT_TRY(saturn::core::require_initialized());
    if (coefficients == nullptr || (byte_address & 3u) != 0u || byte_address + count * 4u > nbg::kVramBytes) {
        return SAT_ERR_INVALID_ARG;
    }
    uint16_t chunk[kChunkWords];
    uint32_t address = byte_address;
    for (uint32_t done = 0u; done < count;) {
        const uint32_t n = count - done < kChunkWords / 2u ? count - done : kChunkWords / 2u;
        for (uint32_t i = 0u; i < n; ++i) {
            chunk[2u * i] = static_cast<uint16_t>(coefficients[done + i] >> 16u);
            chunk[2u * i + 1u] = static_cast<uint16_t>(coefficients[done + i] & 0xFFFFu);
        }
        SAT_TRY(write_table(address, chunk, 2u * n));
        address += n * 4u;
        done += n;
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_rotation_write_coefficients_1word(uint32_t byte_address,
                                                                   const uint16_t* coefficients,
                                                                   uint32_t count) {
    SAT_TRY(saturn::core::require_initialized());
    if (coefficients == nullptr || (byte_address & 1u) != 0u ||
        byte_address + count * 2u > nbg::kVramBytes) {
        return SAT_ERR_INVALID_ARG;
    }
    uint16_t chunk[kChunkWords];
    uint32_t address = byte_address;
    for (uint32_t done = 0u; done < count;) {
        const uint32_t n = count - done < kChunkWords ? count - done : kChunkWords;
        for (uint32_t i = 0u; i < n; ++i) chunk[i] = coefficients[done + i];
        SAT_TRY(write_table(address, chunk, n));
        address += n * 2u;
        done += n;
    }
    return SAT_OK;
}

extern "C" uint32_t sat_vdp2_line_scroll_table_bytes(const sat_vdp2_line_scroll_config_t* config,
                                                     uint32_t lines) {
    if (config == nullptr || config->interval > 3u) return 0u;
    const uint32_t words = nbg::line_scroll_entry_words(config->horizontal != 0u,
                                                        config->vertical != 0u, config->zoom != 0u);
    return nbg::line_scroll_entries(lines, config->interval) * words * 2u;
}

extern "C" sat_result_t sat_vdp2_layer_line_scroll_enable(const sat_vdp2_line_scroll_config_t* config) {
    SAT_TRY(saturn::core::require_initialized());
    if (config == nullptr || static_cast<uint32_t>(config->layer) > 1u || config->interval > 3u ||
        (config->horizontal == 0u && config->vertical == 0u && config->zoom == 0u)) {
        return SAT_ERR_INVALID_ARG;
    }
    nbg::Layer current{};
    if (!hal::nbg_layer(config->layer, &current)) return SAT_ERR_NOT_FOUND;
    current.ls_h = config->horizontal != 0u;
    current.ls_v = config->vertical != 0u;
    current.ls_zoom = config->zoom != 0u;
    current.ls_interval = config->interval;
    current.ls_address = config->table_address;
    return to_result(hal::nbg_configure(config->layer, current));
}

extern "C" sat_result_t sat_vdp2_layer_line_scroll_disable(sat_vdp2_layer_t layer) {
    SAT_TRY(saturn::core::require_initialized());
    if (static_cast<uint32_t>(layer) > 1u) return SAT_ERR_INVALID_ARG;
    nbg::Layer current{};
    if (!hal::nbg_layer(static_cast<uint8_t>(layer), &current)) return SAT_ERR_NOT_FOUND;
    current.ls_h = current.ls_v = current.ls_zoom = false;
    return to_result(hal::nbg_configure(static_cast<uint8_t>(layer), current));
}

extern "C" sat_result_t sat_vdp2_line_scroll_write(sat_vdp2_layer_t layer, uint32_t first_entry,
                                                   const sat_vdp2_line_scroll_entry_t* entries,
                                                   uint32_t count) {
    SAT_TRY(saturn::core::require_initialized());
    if (static_cast<uint32_t>(layer) > 1u || entries == nullptr) return SAT_ERR_INVALID_ARG;
    nbg::Layer l{};
    if (!hal::nbg_layer(static_cast<uint8_t>(layer), &l)) return SAT_ERR_NOT_FOUND;
    const uint32_t per = entry_words(l);
    if (per == 0u) return SAT_ERR_UNSUPPORTED;
    uint16_t chunk[kChunkWords];
    uint32_t used = 0u;
    uint32_t address = l.ls_address + first_entry * per * 2u;
    for (uint32_t i = 0u; i < count; ++i) {
        if (used + per > kChunkWords) {
            SAT_TRY(write_table(address, chunk, used));
            address += used * 2u;
            used = 0u;
        }
        used += encode_entry(l, entries[i], chunk + used);
    }
    if (used != 0u) SAT_TRY(write_table(address, chunk, used));
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_line_scroll_fill_wave(sat_vdp2_layer_t layer, uint32_t lines,
                                                       int32_t amplitude, uint32_t period_lines,
                                                       int32_t phase_degrees) {
    SAT_TRY(saturn::core::require_initialized());
    if (static_cast<uint32_t>(layer) > 1u || period_lines == 0u) return SAT_ERR_INVALID_ARG;
    nbg::Layer l{};
    if (!hal::nbg_layer(static_cast<uint8_t>(layer), &l)) return SAT_ERR_NOT_FOUND;
    if (!l.ls_h) return SAT_ERR_UNSUPPORTED;
    const uint32_t entries = nbg::line_scroll_entries(lines, l.ls_interval);
    const uint32_t step_lines = 1u << l.ls_interval;
    /* 360 degrees per period, in 16.16. */
    const int64_t per_line = (360ll << 16) / static_cast<int64_t>(period_lines);
    for (uint32_t first = 0u; first < entries;) {
        sat_vdp2_line_scroll_entry_t batch[16];
        uint32_t n = 0u;
        for (; n < 16u && first + n < entries; ++n) {
            const int64_t angle = static_cast<int64_t>(phase_degrees) +
                                  per_line * static_cast<int64_t>((first + n) * step_lines);
            const int32_t wrapped = static_cast<int32_t>(angle % (360ll << 16));
            const sat_fx16_t s = sat_sin_deg(wrapped);
            batch[n].x = static_cast<int32_t>((static_cast<int64_t>(s) * amplitude) >> 16);
            batch[n].y = 0;
            batch[n].zoom = 0x10000u;
        }
        SAT_TRY(sat_vdp2_line_scroll_write(layer, first, batch, n));
        first += n;
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_vertical_cell_scroll_write(sat_vdp2_layer_t layer, uint32_t first_cell,
                                                            const int32_t* values, uint32_t count) {
    SAT_TRY(saturn::core::require_initialized());
    if (static_cast<uint32_t>(layer) > 1u || values == nullptr) return SAT_ERR_INVALID_ARG;
    nbg::Layer l{};
    if (!hal::nbg_layer(static_cast<uint8_t>(layer), &l)) return SAT_ERR_NOT_FOUND;
    if (!l.vcs) return SAT_ERR_UNSUPPORTED;
    nbg::Layer other{};
    const bool both = hal::nbg_layer(static_cast<uint8_t>(1u - static_cast<uint32_t>(layer)), &other) &&
                      other.enabled && other.vcs;
    uint16_t pair[2];
    for (uint32_t i = 0u; i < count; ++i) {
        nbg::encode_scroll(values[i], &pair[0], &pair[1]);
        const uint32_t word = nbg::vcs_word_offset(first_cell + i, static_cast<uint8_t>(layer), both);
        SAT_TRY(write_table(l.vcs_address + word * 2u, pair, 2u));
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_back_screen_set_lines(uint32_t table_address, const uint16_t* rgb555,
                                                       uint32_t count) {
    SAT_TRY(saturn::core::require_initialized());
    if (rgb555 == nullptr || count == 0u || (table_address & 1u) != 0u) return SAT_ERR_INVALID_ARG;
    uint16_t chunk[kChunkWords];
    uint32_t address = table_address;
    for (uint32_t done = 0u; done < count;) {
        const uint32_t n = count - done < kChunkWords ? count - done : kChunkWords;
        for (uint32_t i = 0u; i < n; ++i) chunk[i] = static_cast<uint16_t>(rgb555[done + i] & 0x7FFFu);
        SAT_TRY(write_table(address, chunk, n));
        address += n * 2u;
        done += n;
    }
    hal::set_backdrop_lines(table_address / 2u);
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_line_color_screen_set(uint32_t table_address,
                                                       const uint16_t* cram_indices, uint32_t count) {
    SAT_TRY(saturn::core::require_initialized());
    if (cram_indices == nullptr || count == 0u || (table_address & 1u) != 0u) return SAT_ERR_INVALID_ARG;
    uint16_t chunk[kChunkWords];
    uint32_t address = table_address;
    for (uint32_t done = 0u; done < count;) {
        const uint32_t n = count - done < kChunkWords ? count - done : kChunkWords;
        for (uint32_t i = 0u; i < n; ++i) chunk[i] = static_cast<uint16_t>(cram_indices[done + i] & 0x07FFu);
        SAT_TRY(write_table(address, chunk, n));
        address += n * 2u;
        done += n;
    }
    hal::set_line_color_screen(table_address / 2u);
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_sprite_set_line_color_insert(uint8_t enabled) {
    SAT_TRY(saturn::core::require_initialized());
    hal::set_line_color_sprite(enabled != 0u);
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_layer_set_line_color_insert(sat_vdp2_layer_t layer, uint8_t enabled) {
    SAT_TRY(saturn::core::require_initialized());
    if (!valid_layer(layer)) return SAT_ERR_INVALID_ARG;
    /* LNCLEN keeps one bit per screen; the layers own bits 0-3. */
    static uint16_t mask = 0u;
    const uint16_t bit = static_cast<uint16_t>(1u << static_cast<uint32_t>(layer));
    mask = enabled != 0u ? static_cast<uint16_t>(mask | bit) : static_cast<uint16_t>(mask & ~bit);
    hal::set_line_color_layers(mask);
    return SAT_OK;
}
