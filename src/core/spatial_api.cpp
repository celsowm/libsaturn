#include "saturn/spatial.h"
#include "src/core/spatial_logic.hpp"

using namespace saturn::core::spatial;

extern "C" sat_result_t sat_spatial_init(sat_spatial_t* s, uint16_t* heads,
    uint16_t cols, uint16_t rows, uint8_t shift, sat_spatial_entry_t* entries,
    uint16_t entry_cap, uint16_t* stamps, sat_box2_t* items, uint16_t item_cap) {
    if (!s || !heads || !entries || !stamps || !items || !cols || !rows || !item_cap) return SAT_ERR_INVALID_ARG;
    s->heads = heads; s->entries = entries; s->stamps = stamps; s->items = items;
    s->cols = cols; s->rows = rows; s->cell_shift = shift;
    s->entry_cap = entry_cap; s->entry_count = 0; s->item_cap = item_cap; s->query_stamp = 0;
    clear(*s);
    for (uint16_t i = 0; i < item_cap; ++i) stamps[i] = 0;
    return SAT_OK;
}
extern "C" void sat_spatial_clear(sat_spatial_t* s) { if (valid(s)) clear(*s); }
extern "C" sat_result_t sat_spatial_insert(sat_spatial_t* s, uint16_t id, const sat_box2_t* b) {
    return valid(s) && b ? insert(*s, id, *b) : SAT_ERR_INVALID_ARG;
}
extern "C" sat_result_t sat_spatial_query(sat_spatial_t* s, const sat_box2_t* b,
    uint16_t* out, uint16_t cap, uint16_t* count) {
    if (!valid(s) || !b || (!out && cap) || !count) return SAT_ERR_INVALID_ARG;
    uint16_t n = 0; const sat_result_t r = query(*s, *b, out, cap, n); *count = n; return r;
}
extern "C" sat_result_t sat_spatial_pairs(sat_spatial_t* s, sat_spatial_pair_t* out,
    uint16_t cap, uint16_t* count) {
    if (!valid(s) || (!out && cap) || !count) return SAT_ERR_INVALID_ARG;
    uint16_t n = 0; const sat_result_t r = pairs(*s, out, cap, n); *count = n; return r;
}
