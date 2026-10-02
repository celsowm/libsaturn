#include "saturn/entity_stream2.h"

#include <limits.h>

/* Entity region activation: the logic behind include/saturn/entity_stream2.h. */

namespace {

constexpr int64_t kOne = SAT_FX16_ONE;

inline uint32_t words_for(uint32_t bits) { return (bits + 31u) >> 5; }
inline bool test_bit(const uint32_t* bits, uint32_t i) { return (bits[i >> 5] >> (i & 31u)) & 1u; }
inline void set_bit(uint32_t* bits, uint32_t i) { bits[i >> 5] |= 1u << (i & 31u); }
inline void clear_bit(uint32_t* bits, uint32_t i) { bits[i >> 5] &= ~(1u << (i & 31u)); }

/* floor(v / 65536) and the matching ceiling, for any sign. */
inline int64_t floor_px(int64_t v) { return v >> 16; }
inline int64_t ceil_px(int64_t v) { return (v + (kOne - 1)) >> 16; }

struct Edges {
    int64_t min_x, min_y, max_x, max_y; /* 16.16 world pixels, half-open */
};

Edges edges_of(const sat_box2_t& b, int64_t grow) {
    return {static_cast<int64_t>(b.center.x) - b.half.x - grow, static_cast<int64_t>(b.center.y) - b.half.y - grow,
            static_cast<int64_t>(b.center.x) + b.half.x + grow, static_cast<int64_t>(b.center.y) + b.half.y + grow};
}

inline bool inside(const sat_entity_index2_t& idx, const sat_entity_desc2_t& d, const Edges& e) {
    const int64_t px = (static_cast<int64_t>(idx.origin_x) + d.x) * kOne;
    const int64_t py = (static_cast<int64_t>(idx.origin_y) + d.y) * kOne;
    return px >= e.min_x && px < e.max_x && py >= e.min_y && py < e.max_y;
}

}  // namespace

extern "C" sat_result_t sat_entity_index2_validate(const sat_entity_index2_t* index) {
    if (!index || !index->region_start || (index->desc_count && !index->descs)) return SAT_ERR_INVALID_ARG;
    if (index->region_cols == 0 || index->region_rows == 0 || index->region_shift < 3 || index->region_shift > 15) return SAT_ERR_INVALID_ARG;
    const uint32_t regions = static_cast<uint32_t>(index->region_cols) * index->region_rows;
    if (index->region_start[0] != 0 || index->region_start[regions] != index->desc_count) return SAT_ERR_INVALID_ARG;
    for (uint32_t r = 0; r < regions; ++r) {
        const uint32_t first = index->region_start[r], last = index->region_start[r + 1];
        if (last < first) return SAT_ERR_INVALID_ARG;
        const uint32_t col = r % index->region_cols, row = r / index->region_cols;
        for (uint32_t i = first; i < last; ++i) {
            const sat_entity_desc2_t& d = index->descs[i];
            if ((static_cast<uint32_t>(d.x) >> index->region_shift) != col || (static_cast<uint32_t>(d.y) >> index->region_shift) != row)
                return SAT_ERR_INVALID_ARG;
        }
    }
    return SAT_OK;
}

extern "C" uint32_t sat_entity_index2_bytes(uint32_t desc_count, uint32_t region_count) {
    return desc_count * static_cast<uint32_t>(sizeof(sat_entity_desc2_t)) + (region_count + 1u) * static_cast<uint32_t>(sizeof(uint16_t));
}

extern "C" uint32_t sat_entity_stream2_state_words(uint32_t desc_count) { return 2u * words_for(desc_count); }

extern "C" sat_result_t sat_entity_stream2_init(sat_entity_stream2_t* stream, const sat_entity_stream2_config_t* config) {
    if (!stream || !config || !config->index || !config->activate || config->hysteresis < 0) return SAT_ERR_INVALID_ARG;
    if (sat_entity_index2_validate(config->index) != SAT_OK) return SAT_ERR_INVALID_ARG;
    const uint32_t bit_words = words_for(config->index->desc_count);
    if (config->state_words < 2u * bit_words || (bit_words > 0 && !config->state)) return SAT_ERR_INVALID_ARG;
    *stream = {};
    stream->index = *config->index;
    stream->bit_words = bit_words;
    stream->active = config->state;
    stream->retired = config->state + bit_words;
    for (uint32_t i = 0; i < 2u * bit_words; ++i) config->state[i] = 0;
    stream->hysteresis = config->hysteresis;
    stream->activate = config->activate;
    stream->deactivate = config->deactivate;
    stream->user = config->user;
    return SAT_OK;
}

extern "C" sat_result_t sat_entity_stream2_update(sat_entity_stream2_t* stream, const sat_box2_t* box, sat_entity_stream2_result_t* out) {
    if (!stream || !box || box->half.x < 0 || box->half.y < 0) return SAT_ERR_INVALID_ARG;
    const sat_entity_index2_t& idx = stream->index;
    sat_entity_stream2_result_t result = {};

    /* 1. release what has left the grown box, lowest index first */
    if (stream->active_count > 0) {
        const Edges grown = edges_of(*box, stream->hysteresis);
        for (uint32_t wi = 0; wi < stream->bit_words && stream->active_count > 0; ++wi) {
            uint32_t done = 0; /* bits of this word already looked at */
            for (;;) {
                const uint32_t pending = stream->active[wi] & ~done;
                if (!pending) break;
                uint32_t b = 0;
                while (!((pending >> b) & 1u)) ++b;
                done |= (b == 31u) ? 0xFFFFFFFFu : ((2u << b) - 1u); /* this bit and every lower one */
                const uint32_t i = (wi << 5) + b;
                const sat_entity_desc2_t& d = idx.descs[i];
                if (inside(idx, d, grown)) continue;
                clear_bit(stream->active, i);
                --stream->active_count;
                ++stream->stats.deactivations;
                ++result.deactivated;
                if (stream->deactivate) stream->deactivate(stream->user, i, &d);
            }
        }
    }

    /* 2. wake what is inside the box, lowest index first */
    const Edges e = edges_of(*box, 0);
    const int64_t span_x = static_cast<int64_t>(idx.region_cols) << idx.region_shift;
    const int64_t span_y = static_cast<int64_t>(idx.region_rows) << idx.region_shift;
    const int64_t lo_x = floor_px(e.min_x) - idx.origin_x, hi_x = ceil_px(e.max_x) - 1 - idx.origin_x;
    const int64_t lo_y = floor_px(e.min_y) - idx.origin_y, hi_y = ceil_px(e.max_y) - 1 - idx.origin_y;
    if (hi_x >= 0 && lo_x < span_x && hi_y >= 0 && lo_y < span_y && e.max_x > e.min_x && e.max_y > e.min_y) {
        const uint32_t c0 = static_cast<uint32_t>((lo_x < 0 ? 0 : lo_x) >> idx.region_shift);
        const uint32_t c1 = static_cast<uint32_t>((hi_x >= span_x ? span_x - 1 : hi_x) >> idx.region_shift);
        const uint32_t r0 = static_cast<uint32_t>((lo_y < 0 ? 0 : lo_y) >> idx.region_shift);
        const uint32_t r1 = static_cast<uint32_t>((hi_y >= span_y ? span_y - 1 : hi_y) >> idx.region_shift);
        bool stop = false;
        for (uint32_t row = r0; row <= r1 && !stop; ++row) {
            for (uint32_t col = c0; col <= c1 && !stop; ++col) {
                const uint32_t region = row * idx.region_cols + col;
                for (uint32_t i = idx.region_start[region]; i < idx.region_start[region + 1]; ++i) {
                    if (test_bit(stream->active, i) || test_bit(stream->retired, i)) continue;
                    const sat_entity_desc2_t& d = idx.descs[i];
                    if (!inside(idx, d, e)) continue;
                    const sat_entity_activate_result_t r = stream->activate(stream->user, i, &d);
                    if (r == SAT_ENTITY_DEFER) {
                        result.deferred = 1;
                        ++stream->stats.deferrals;
                        stop = true;
                        break;
                    }
                    if (r == SAT_ENTITY_DECLINE) {
                        set_bit(stream->retired, i);
                        ++stream->stats.declines;
                        continue;
                    }
                    if (test_bit(stream->retired, i)) continue; /* the callback retired it itself */
                    set_bit(stream->active, i);
                    ++stream->active_count;
                    ++stream->stats.activations;
                    ++result.activated;
                    if (stream->active_count > stream->stats.peak_active) stream->stats.peak_active = stream->active_count;
                }
            }
        }
    }
    ++stream->stats.updates;
    if (out) *out = result;
    return SAT_OK;
}

extern "C" int sat_entity_stream2_is_active(const sat_entity_stream2_t* stream, uint32_t index) {
    return stream && index < stream->index.desc_count && test_bit(stream->active, index) ? 1 : 0;
}

extern "C" int sat_entity_stream2_is_retired(const sat_entity_stream2_t* stream, uint32_t index) {
    return stream && index < stream->index.desc_count && test_bit(stream->retired, index) ? 1 : 0;
}

extern "C" uint32_t sat_entity_stream2_active_count(const sat_entity_stream2_t* stream) { return stream ? stream->active_count : 0; }

extern "C" sat_result_t sat_entity_stream2_release(sat_entity_stream2_t* stream, uint32_t index) {
    if (!stream || index >= stream->index.desc_count) return SAT_ERR_INVALID_ARG;
    if (test_bit(stream->active, index)) {
        clear_bit(stream->active, index);
        --stream->active_count;
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_entity_stream2_retire(sat_entity_stream2_t* stream, uint32_t index) {
    const sat_result_t r = sat_entity_stream2_release(stream, index);
    if (r != SAT_OK) return r;
    set_bit(stream->retired, index);
    return SAT_OK;
}

extern "C" sat_result_t sat_entity_stream2_unretire(sat_entity_stream2_t* stream, uint32_t index) {
    if (!stream || index >= stream->index.desc_count) return SAT_ERR_INVALID_ARG;
    clear_bit(stream->retired, index);
    return SAT_OK;
}

extern "C" void sat_entity_stream2_reset(sat_entity_stream2_t* stream) {
    if (!stream) return;
    for (uint32_t i = 0; i < 2u * stream->bit_words; ++i) stream->active[i] = 0; /* active and retired are contiguous */
    stream->active_count = 0;
}

extern "C" sat_entity_stream2_stats_t sat_entity_stream2_stats(const sat_entity_stream2_t* stream) {
    return stream ? stream->stats : sat_entity_stream2_stats_t{};
}
