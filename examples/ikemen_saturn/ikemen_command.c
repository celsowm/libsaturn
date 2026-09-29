#include "ikemen_command.h"

#include <string.h>

enum {
    IN_U = 0,
    IN_D,
    IN_L,
    IN_R,
    IN_B,
    IN_F,
    IN_N,
    IN_A,
    IN_b,
    IN_C,
    IN_X,
    IN_Y,
    IN_Z,
    IN_S,
    IN_d,
    IN_W,
    IN_M,
    IN_COUNT
};

static int16_t iabs16(int16_t v) { return v < 0 ? (int16_t)-v : v; }
static int16_t imin16(int16_t a, int16_t b) { return a < b ? a : b; }
static int16_t imax16(int16_t a, int16_t b) { return a > b ? a : b; }

static int16_t min4(int16_t a, int16_t b, int16_t c, int16_t d) {
    return imin16(imin16(a, b), imin16(c, d));
}

static void update_counter(int16_t* current, int16_t* previous, int held) {
    *previous = *current;
    if (held != (*current > 0)) {
        *current = held ? 1 : -1;
        return;
    }
    if (held) {
        if (*current < 32760) ++*current;
    } else {
        if (*current > -32760) --*current;
    }
}

static void input_update(ik_command_input_state_t* in,
                         const sat_pad_state_t* pad,
                         int facing) {
    const uint16_t held = pad ? pad->held : 0u;
    int u = (held & SAT_PAD_UP) != 0u;
    int d = (held & SAT_PAD_DOWN) != 0u;
    int l = (held & SAT_PAD_LEFT) != 0u;
    int r = (held & SAT_PAD_RIGHT) != 0u;

    /* KFM's CMD is expressed in B/F, not screen L/R. Ikemen resolves those
     * against the character's current facing before updating its input buffer. */
    int back = facing < 0 ? r : l;
    int fwd = facing < 0 ? l : r;
    int neutral = !(u || d || l || r);

    const int values[IN_COUNT] = {
        u, d, l, r, back, fwd, neutral,
        (held & SAT_PAD_A) != 0u,
        (held & SAT_PAD_B) != 0u,
        (held & SAT_PAD_C) != 0u,
        (held & SAT_PAD_X) != 0u,
        (held & SAT_PAD_Y) != 0u,
        (held & SAT_PAD_Z) != 0u,
        (held & SAT_PAD_START) != 0u,
        0, 0, 0
    };
    for (int i = 0; i < IN_COUNT; ++i) {
        update_counter(&in->current[i], &in->previous[i], values[i]);
    }
}

static int16_t base_current(const ik_command_input_state_t* in, uint8_t key) {
    switch (key) {
        case IK_CMD_KEY_U: return in->current[IN_U];
        case IK_CMD_KEY_D: return in->current[IN_D];
        case IK_CMD_KEY_B: return in->current[IN_B];
        case IK_CMD_KEY_F: return in->current[IN_F];
        case IK_CMD_KEY_L: return in->current[IN_L];
        case IK_CMD_KEY_R: return in->current[IN_R];
        case IK_CMD_KEY_N: return in->current[IN_N];
        case IK_CMD_KEY_A: return in->current[IN_A];
        case IK_CMD_KEY_BTN_B: return in->current[IN_b];
        case IK_CMD_KEY_C: return in->current[IN_C];
        case IK_CMD_KEY_X: return in->current[IN_X];
        case IK_CMD_KEY_Y: return in->current[IN_Y];
        case IK_CMD_KEY_Z: return in->current[IN_Z];
        case IK_CMD_KEY_S: return in->current[IN_S];
        case IK_CMD_KEY_BTN_D: return in->current[IN_d];
        case IK_CMD_KEY_W: return in->current[IN_W];
        case IK_CMD_KEY_M: return in->current[IN_M];
        default: return 0;
    }
}

static int16_t base_previous(const ik_command_input_state_t* in, uint8_t key) {
    switch (key) {
        case IK_CMD_KEY_U: return in->previous[IN_U];
        case IK_CMD_KEY_D: return in->previous[IN_D];
        case IK_CMD_KEY_B: return in->previous[IN_B];
        case IK_CMD_KEY_F: return in->previous[IN_F];
        case IK_CMD_KEY_L: return in->previous[IN_L];
        case IK_CMD_KEY_R: return in->previous[IN_R];
        case IK_CMD_KEY_N: return in->previous[IN_N];
        case IK_CMD_KEY_A: return in->previous[IN_A];
        case IK_CMD_KEY_BTN_B: return in->previous[IN_b];
        case IK_CMD_KEY_C: return in->previous[IN_C];
        case IK_CMD_KEY_X: return in->previous[IN_X];
        case IK_CMD_KEY_Y: return in->previous[IN_Y];
        case IK_CMD_KEY_Z: return in->previous[IN_Z];
        case IK_CMD_KEY_S: return in->previous[IN_S];
        case IK_CMD_KEY_BTN_D: return in->previous[IN_d];
        case IK_CMD_KEY_W: return in->previous[IN_W];
        case IK_CMD_KEY_M: return in->previous[IN_M];
        default: return 0;
    }
}

static int is_button(uint8_t key) {
    return key >= IK_CMD_KEY_A && key <= IK_CMD_KEY_M;
}

static int is_direction(uint8_t key) {
    return key <= IK_CMD_KEY_N;
}

static void direction_components(uint8_t key, uint8_t* a, uint8_t* b, int* count) {
    *count = 1;
    *a = key;
    *b = 0u;
    switch (key) {
        case IK_CMD_KEY_UB: *a = IK_CMD_KEY_U; *b = IK_CMD_KEY_B; *count = 2; break;
        case IK_CMD_KEY_UF: *a = IK_CMD_KEY_U; *b = IK_CMD_KEY_F; *count = 2; break;
        case IK_CMD_KEY_DB: *a = IK_CMD_KEY_D; *b = IK_CMD_KEY_B; *count = 2; break;
        case IK_CMD_KEY_DF: *a = IK_CMD_KEY_D; *b = IK_CMD_KEY_F; *count = 2; break;
        case IK_CMD_KEY_UL: *a = IK_CMD_KEY_U; *b = IK_CMD_KEY_L; *count = 2; break;
        case IK_CMD_KEY_UR: *a = IK_CMD_KEY_U; *b = IK_CMD_KEY_R; *count = 2; break;
        case IK_CMD_KEY_DL: *a = IK_CMD_KEY_D; *b = IK_CMD_KEY_L; *count = 2; break;
        case IK_CMD_KEY_DR: *a = IK_CMD_KEY_D; *b = IK_CMD_KEY_R; *count = 2; break;
        default: break;
    }
}

static int16_t strict_direction_state(const ik_command_input_state_t* in,
                                      uint8_t key) {
    const int16_t U = in->current[IN_U], D = in->current[IN_D];
    const int16_t B = in->current[IN_B], F = in->current[IN_F];
    const int16_t L = in->current[IN_L], R = in->current[IN_R];
    switch (key) {
        case IK_CMD_KEY_U: return imin16((int16_t)-imax16(B, imax16(D, F)), U);
        case IK_CMD_KEY_D: return imin16((int16_t)-imax16(B, imax16(U, F)), D);
        case IK_CMD_KEY_B: return imin16((int16_t)-imax16(D, imax16(U, F)), B);
        case IK_CMD_KEY_F: return imin16((int16_t)-imax16(D, imax16(U, B)), F);
        case IK_CMD_KEY_L: return imin16((int16_t)-imax16(D, imax16(U, R)), L);
        case IK_CMD_KEY_R: return imin16((int16_t)-imax16(D, imax16(U, L)), R);
        case IK_CMD_KEY_UB: return imin16((int16_t)-imax16(D, F), imin16(U, B));
        case IK_CMD_KEY_UF: return imin16((int16_t)-imax16(D, B), imin16(U, F));
        case IK_CMD_KEY_DB: return imin16((int16_t)-imax16(U, F), imin16(D, B));
        case IK_CMD_KEY_DF: return imin16((int16_t)-imax16(U, B), imin16(D, F));
        case IK_CMD_KEY_UL: return imin16((int16_t)-imax16(D, R), imin16(U, L));
        case IK_CMD_KEY_UR: return imin16((int16_t)-imax16(D, L), imin16(U, R));
        case IK_CMD_KEY_DL: return imin16((int16_t)-imax16(U, R), imin16(D, L));
        case IK_CMD_KEY_DR: return imin16((int16_t)-imax16(U, L), imin16(D, R));
        case IK_CMD_KEY_N: return in->current[IN_N];
        default: return 0;
    }
}

static int16_t dollar_direction_state(const ik_command_input_state_t* in,
                                      uint8_t key) {
    uint8_t a, b;
    int count;
    direction_components(key, &a, &b, &count);
    if (key == IK_CMD_KEY_N) {
        int16_t v = 32760;
        for (int i = 0; i < IN_COUNT; ++i) {
            v = imin16(v, iabs16(in->current[i]));
        }
        return v;
    }

    if (count == 1) {
        const int16_t v = base_current(in, a);
        if (v <= 0) return 0;
    } else {
        if (base_current(in, a) <= 0 || base_current(in, b) <= 0) return 0;
    }

    if (key == IK_CMD_KEY_L || key == IK_CMD_KEY_R ||
        key == IK_CMD_KEY_UL || key == IK_CMD_KEY_UR ||
        key == IK_CMD_KEY_DL || key == IK_CMD_KEY_DR) {
        return min4(iabs16(in->current[IN_U]), iabs16(in->current[IN_D]),
                    iabs16(in->current[IN_L]), iabs16(in->current[IN_R]));
    }
    return min4(iabs16(in->current[IN_U]), iabs16(in->current[IN_D]),
                iabs16(in->current[IN_B]), iabs16(in->current[IN_F]));
}

static int16_t release_direction_state(const ik_command_input_state_t* in,
                                       uint8_t key,
                                       int dollar) {
    uint8_t a, b;
    int count;
    direction_components(key, &a, &b, &count);

    if (dollar) {
        if (count == 1) {
            int16_t cur = base_current(in, a);
            int16_t prev = base_previous(in, a);
            if (!(cur < 0 || prev > 0)) return 0;
            if (cur < 0) return (int16_t)-cur;
            if (a == IK_CMD_KEY_U)
                return imin16(iabs16(in->current[IN_D]), imin16(iabs16(in->current[IN_B]), iabs16(in->current[IN_F])));
            if (a == IK_CMD_KEY_D)
                return imin16(iabs16(in->current[IN_U]), imin16(iabs16(in->current[IN_B]), iabs16(in->current[IN_F])));
            if (a == IK_CMD_KEY_B)
                return imin16(iabs16(in->current[IN_U]), imin16(iabs16(in->current[IN_D]), iabs16(in->current[IN_F])));
            if (a == IK_CMD_KEY_F)
                return imin16(iabs16(in->current[IN_U]), imin16(iabs16(in->current[IN_D]), iabs16(in->current[IN_B])));
            if (a == IK_CMD_KEY_L)
                return imin16(iabs16(in->current[IN_U]), imin16(iabs16(in->current[IN_D]), iabs16(in->current[IN_R])));
            if (a == IK_CMD_KEY_R)
                return imin16(iabs16(in->current[IN_U]), imin16(iabs16(in->current[IN_D]), iabs16(in->current[IN_L])));
            return 0;
        }
        if ((base_current(in, a) < 0 || base_previous(in, a) > 0) &&
            (base_current(in, b) < 0 || base_previous(in, b) > 0)) {
            int16_t x = base_current(in, a);
            int16_t y = base_current(in, b);
            if (x < 0 || y < 0) return (int16_t)-imin16(x, y);
            return 1;
        }
        return 0;
    }

    /* This mirrors Ikemen's release-direction behavior. A release step can
     * become true while the direction is still physically held if a newly
     * conflicting direction changes the strict direction state. That is what
     * makes canonical "~D, DF, F" quarter-circles work. */
    if (count == 1) {
        const int16_t cur = base_current(in, a);
        const int16_t prev = base_previous(in, a);
        if (!(cur < 0 || prev > 0)) return 0;
        if (a == IK_CMD_KEY_N) return (int16_t)-cur;

        int16_t conflict = 0;
        if (a == IK_CMD_KEY_U) conflict = (int16_t)-imax16(in->current[IN_B], imax16(in->current[IN_D], in->current[IN_F]));
        else if (a == IK_CMD_KEY_D) conflict = (int16_t)-imax16(in->current[IN_B], imax16(in->current[IN_U], in->current[IN_F]));
        else if (a == IK_CMD_KEY_B) conflict = (int16_t)-imax16(in->current[IN_D], imax16(in->current[IN_U], in->current[IN_F]));
        else if (a == IK_CMD_KEY_F) conflict = (int16_t)-imax16(in->current[IN_D], imax16(in->current[IN_U], in->current[IN_B]));
        else if (a == IK_CMD_KEY_L) conflict = (int16_t)-imax16(in->current[IN_D], imax16(in->current[IN_U], in->current[IN_R]));
        else if (a == IK_CMD_KEY_R) conflict = (int16_t)-imax16(in->current[IN_D], imax16(in->current[IN_U], in->current[IN_L]));
        return (int16_t)-imin16(conflict, cur);
    }

    if (!((base_current(in, a) < 0 || base_previous(in, a) > 0) &&
          (base_current(in, b) < 0 || base_previous(in, b) > 0))) {
        return 0;
    }

    int16_t conflict = 0;
    switch (key) {
        case IK_CMD_KEY_UB: conflict = (int16_t)-imax16(in->current[IN_D], in->current[IN_F]); break;
        case IK_CMD_KEY_UF: conflict = (int16_t)-imax16(in->current[IN_D], in->current[IN_B]); break;
        case IK_CMD_KEY_DB: conflict = (int16_t)-imax16(in->current[IN_U], in->current[IN_F]); break;
        case IK_CMD_KEY_DF: conflict = (int16_t)-imax16(in->current[IN_U], in->current[IN_B]); break;
        case IK_CMD_KEY_UL: conflict = (int16_t)-imax16(in->current[IN_D], in->current[IN_R]); break;
        case IK_CMD_KEY_UR: conflict = (int16_t)-imax16(in->current[IN_D], in->current[IN_L]); break;
        case IK_CMD_KEY_DL: conflict = (int16_t)-imax16(in->current[IN_U], in->current[IN_R]); break;
        case IK_CMD_KEY_DR: conflict = (int16_t)-imax16(in->current[IN_U], in->current[IN_L]); break;
        default: break;
    }
    return (int16_t)-imin16(conflict,
                             imin16(base_current(in, a), base_current(in, b)));
}

static int16_t input_state(const ik_command_input_state_t* in,
                           const ik_cmd_key_spec_t* spec) {
    const int tilde = (spec->flags & IK_CMD_KEY_TILDE) != 0u;
    const int dollar = (spec->flags & IK_CMD_KEY_DOLLAR) != 0u;
    if (is_direction(spec->key)) {
        if (tilde) return release_direction_state(in, spec->key, dollar);
        if (dollar) return dollar_direction_state(in, spec->key);
        return strict_direction_state(in, spec->key);
    }

    if (is_button(spec->key)) {
        int16_t cur = base_current(in, spec->key);
        if (!tilde) return cur;
        if (cur < 0 || base_previous(in, spec->key) > 0) return (int16_t)-cur;
    }
    return 0;
}

static int16_t input_charge(const ik_command_input_state_t* in,
                            const ik_cmd_key_spec_t* spec) {
    if (spec->charge_time <= 1u) return 32760;
    if ((spec->flags & IK_CMD_KEY_TILDE) != 0u) {
        int16_t p = base_previous(in, spec->key);
        return p > 0 ? p : 0;
    }
    {
        int16_t c = base_current(in, spec->key);
        return c > 0 ? c : 0;
    }
}

static int step_allows(const ik_command_asset_t* asset,
                       const ik_cmd_step_t* step,
                       uint8_t key,
                       int tilde) {
    for (uint8_t i = 0u; i < step->key_count; ++i) {
        const ik_cmd_key_spec_t* spec = &asset->keys[step->key_ofs + i];
        if (spec->key == key &&
            (((spec->flags & IK_CMD_KEY_TILDE) != 0u) == (tilde != 0))) {
            return 1;
        }
    }
    return 0;
}

static int greater_check_fail(const ik_command_input_state_t* in,
                              const ik_command_asset_t* asset,
                              const ik_cmd_step_t* step) {
    static const uint8_t candidates[] = {
        IK_CMD_KEY_U, IK_CMD_KEY_D, IK_CMD_KEY_B, IK_CMD_KEY_F,
        IK_CMD_KEY_UB, IK_CMD_KEY_UF, IK_CMD_KEY_DB, IK_CMD_KEY_DF,
        IK_CMD_KEY_A, IK_CMD_KEY_BTN_B, IK_CMD_KEY_C,
        IK_CMD_KEY_X, IK_CMD_KEY_Y, IK_CMD_KEY_Z, IK_CMD_KEY_S
    };
    for (uint32_t i = 0u; i < sizeof(candidates); ++i) {
        ik_cmd_key_spec_t probe = {candidates[i], 0u, 0u, 0u};
        if (input_state(in, &probe) == 1 &&
            !step_allows(asset, step, candidates[i], 0)) return 1;
        probe.flags = IK_CMD_KEY_TILDE;
        if (input_state(in, &probe) == 1 &&
            !step_allows(asset, step, candidates[i], 1)) return 1;
    }
    return 0;
}

static void clear_pattern(ik_command_pattern_state_t* st, int clear_buffer) {
    const uint8_t keep = clear_buffer ? 0u : st->buffer_time;
    memset(st, 0, sizeof(*st));
    st->buffer_time = keep;
}

static int step_matches(const ik_command_input_state_t* in,
                        const ik_command_asset_t* asset,
                        const ik_cmd_step_t* step) {
    int matched = (step->flags & IK_CMD_STEP_OR) != 0u ? 0 : 1;
    for (uint8_t i = 0u; i < step->key_count; ++i) {
        const ik_cmd_key_spec_t* key = &asset->keys[step->key_ofs + i];
        const int16_t value = input_state(in, key);
        int ok = (key->flags & IK_CMD_KEY_SLASH) != 0u ? value > 0 : value == 1;
        if (ok && key->charge_time > 1u &&
            input_charge(in, key) < (int16_t)key->charge_time) ok = 0;

        if ((step->flags & IK_CMD_STEP_OR) != 0u) {
            if (ok) return 1;
        } else if (!ok) {
            return 0;
        }
    }
    return matched;
}

static int step_pattern(ik_command_state_t* state,
                        const ik_command_asset_t* asset,
                        uint16_t pattern_index,
                        int hit_pause) {
    const ik_cmd_pattern_t* pattern = &asset->patterns[pattern_index];
    ik_command_pattern_state_t* ps = &state->patterns[pattern_index];

    if (ps->buffer_time > 0u &&
        !(hit_pause && (pattern->flags & IK_CMD_PATTERN_BUFFER_HITPAUSE))) {
        --ps->buffer_time;
    }

    int any_done = 0;
    for (uint8_t i = 0u; i < pattern->step_count; ++i) {
        const uint16_t bit = (uint16_t)(1u << i);
        if ((ps->completed_mask & bit) == 0u) continue;
        if (ps->step_timer[i] < 255u) ++ps->step_timer[i];
        if (pattern->max_step_time > 0u &&
            ps->step_timer[i] > pattern->max_step_time) {
            ps->completed_mask = (uint16_t)(ps->completed_mask & ~bit);
            ps->step_timer[i] = 0u;
        } else {
            any_done = 1;
        }
    }

    if (any_done) {
        if (ps->cur_time < 255u) ++ps->cur_time;
    } else if (ps->cur_time > 0u) {
        clear_pattern(ps, 0);
    }

    for (uint8_t oi = 0u; oi < pattern->step_count; ++oi) {
        const uint8_t i = asset->loop_order[pattern->loop_ofs + oi];
        if (i >= pattern->step_count) continue;
        if (i > 0u && (ps->completed_mask & (uint16_t)(1u << (i - 1u))) == 0u) {
            continue;
        }

        const ik_cmd_step_t* step = &asset->steps[pattern->step_ofs + i];
        int matched = step_matches(&state->input, asset, step);

        if ((step->flags & IK_CMD_STEP_GREATER) != 0u && i > 0u &&
            (ps->completed_mask & (uint16_t)(1u << (i - 1u))) != 0u &&
            (ps->completed_mask & (uint16_t)(1u << i)) == 0u) {
            if (greater_check_fail(&state->input, asset, step)) {
                matched = 0;
                ps->completed_mask = (uint16_t)(
                    ps->completed_mask & ~(uint16_t)(1u << (i - 1u)));
                ps->step_timer[i - 1u] = 0u;
            }
        }

        if (matched) {
            ps->completed_mask = (uint16_t)(ps->completed_mask | (uint16_t)(1u << i));
            ps->step_timer[i] = 0u;
            if (i > 0u) {
                ps->completed_mask = (uint16_t)(
                    ps->completed_mask & ~(uint16_t)(1u << (i - 1u)));
                ps->step_timer[i - 1u] = 0u;
            }
            if (i == 0u) ps->cur_time = 0u;
        }
    }

    const uint16_t last_bit =
        pattern->step_count == 0u ? 0u :
        (uint16_t)(1u << (pattern->step_count - 1u));
    const int complete = last_bit != 0u && (ps->completed_mask & last_bit) != 0u;

    if (!complete && ps->cur_time < pattern->max_time) return 0;

    clear_pattern(ps, 0);
    if (complete && ps->buffer_time < pattern->buffer_time) {
        ps->buffer_time = pattern->buffer_time;
    }
    return complete;
}

void ik_command_state_init(ik_command_state_t* state) {
    if (state) memset(state, 0, sizeof(*state));
}

uint16_t ik_command_find(const ik_command_asset_t* asset, const char* name) {
    if (!asset || !name) return IK_CMD_INVALID_ID;
    for (uint16_t i = 0u; i < asset->name_count; ++i) {
        if (asset->names[i].name && strcmp(asset->names[i].name, name) == 0) return i;
    }
    return IK_CMD_INVALID_ID;
}

int ik_command_active(const ik_command_state_t* state,
                      const ik_command_asset_t* asset,
                      uint16_t name_id) {
    if (!state || !asset || name_id >= asset->name_count) return 0;
    const ik_cmd_name_t* name = &asset->names[name_id];
    for (uint8_t i = 0u; i < name->pattern_count; ++i) {
        const uint16_t p = (uint16_t)(name->pattern_ofs + i);
        if (p < asset->pattern_count && p < IK_CMD_MAX_PATTERNS &&
            state->patterns[p].buffer_time > 0u) return 1;
    }
    return 0;
}

void ik_command_update(ik_command_state_t* state,
                       const ik_command_asset_t* asset,
                       const sat_pad_state_t* pad,
                       int facing,
                       int hit_pause) {
    if (!state || !asset || asset->pattern_count > IK_CMD_MAX_PATTERNS) return;
    input_update(&state->input, pad, facing);

    uint64_t completed_names = 0u;
    for (uint16_t p = 0u; p < asset->pattern_count; ++p) {
        if (step_pattern(state, asset, p, hit_pause)) {
            const uint16_t name_id = asset->patterns[p].name_id;
            if (name_id < 64u) completed_names |= (uint64_t)1u << name_id;
        }
    }

    /* Ikemen clears in-progress duplicate variants of a command name after one
     * variant completes, while preserving any already-buffered completion. */
    for (uint16_t n = 0u; n < asset->name_count && n < 64u; ++n) {
        if ((completed_names & ((uint64_t)1u << n)) == 0u) continue;
        const ik_cmd_name_t* name = &asset->names[n];
        for (uint8_t i = 0u; i < name->pattern_count; ++i) {
            const uint16_t p = (uint16_t)(name->pattern_ofs + i);
            if (p < asset->pattern_count) clear_pattern(&state->patterns[p], 0);
        }
    }
}


static int eval_state_rule(const ik_command_state_t* state,
                           const ik_command_asset_t* commands,
                           const ik_state_rule_asset_t* asset,
                           const ik_state_rule_t* rule,
                           const ik_state_rule_context_t* context) {
    int stack[24];
    uint8_t sp = 0u;
    if (!state || !commands || !asset || !rule || !context) return 0;

    for (uint8_t i = 0u; i < rule->instruction_count; ++i) {
        const uint16_t index = (uint16_t)(rule->instruction_ofs + i);
        if (index >= asset->instruction_count) return 0;
        const ik_state_rule_instr_t* ins = &asset->instructions[index];
        int value = 0;

        switch ((ik_cmd_rule_op_t)ins->op) {
            case IK_CMD_RULE_COMMAND_ACTIVE:
                value = ik_command_active(state, commands, (uint16_t)ins->value0);
                break;
            case IK_CMD_RULE_COMMAND_INACTIVE:
                value = !ik_command_active(state, commands, (uint16_t)ins->value0);
                break;
            case IK_CMD_RULE_STATE_TYPE_EQ:
                value = context->state_type == (uint8_t)ins->value0;
                break;
            case IK_CMD_RULE_STATE_TYPE_NE:
                value = context->state_type != (uint8_t)ins->value0;
                break;
            case IK_CMD_RULE_STATE_NO_EQ:
                value = context->state_no == ins->value0;
                break;
            case IK_CMD_RULE_STATE_NO_NE:
                value = context->state_no != ins->value0;
                break;
            case IK_CMD_RULE_STATE_NO_RANGE:
                value = context->state_no >= ins->value0 &&
                        context->state_no <= ins->value1;
                break;
            case IK_CMD_RULE_STATE_TIME_EQ:
                value = context->state_time == (uint16_t)ins->value0;
                break;
            case IK_CMD_RULE_STATE_TIME_GT:
                value = context->state_time > (uint16_t)ins->value0;
                break;
            case IK_CMD_RULE_STATE_TIME_GE:
                value = context->state_time >= (uint16_t)ins->value0;
                break;
            case IK_CMD_RULE_STATE_TIME_LT:
                value = context->state_time < (uint16_t)ins->value0;
                break;
            case IK_CMD_RULE_STATE_TIME_LE:
                value = context->state_time <= (uint16_t)ins->value0;
                break;
            case IK_CMD_RULE_CTRL:
                value = context->ctrl != 0u;
                break;
            case IK_CMD_RULE_MOVE_CONTACT:
                value = context->move_contact != 0u;
                break;
            case IK_CMD_RULE_NOT:
                if (sp < 1u) return 0;
                stack[sp - 1u] = !stack[sp - 1u];
                continue;
            case IK_CMD_RULE_AND:
                if (sp < 2u) return 0;
                stack[sp - 2u] = stack[sp - 2u] && stack[sp - 1u];
                --sp;
                continue;
            case IK_CMD_RULE_OR:
                if (sp < 2u) return 0;
                stack[sp - 2u] = stack[sp - 2u] || stack[sp - 1u];
                --sp;
                continue;
            default:
                return 0;
        }

        if (sp >= (uint8_t)(sizeof(stack) / sizeof(stack[0]))) return 0;
        stack[sp++] = value;
    }
    return sp == 1u && stack[0] != 0;
}

int ik_command_eval_state_change(const ik_command_state_t* state,
                                 const ik_command_asset_t* commands,
                                 const ik_state_rule_asset_t* rules,
                                 const ik_state_rule_context_t* context,
                                 int16_t* out_state) {
    if (!state || !commands || !rules || !context || !out_state) return 0;
    for (uint16_t i = 0u; i < rules->rule_count; ++i) {
        if (eval_state_rule(state, commands, rules, &rules->rules[i], context)) {
            *out_state = rules->rules[i].target_state;
            return 1;
        }
    }
    return 0;
}
