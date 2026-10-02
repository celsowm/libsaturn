#include "saturn/physics2_world.h"

#include <limits.h>

#include "src/core/math2d/logic.hpp"
#include "src/physics/2d/collision_logic.hpp"

/* Physics2 world: stable collider handles, moving support and sensor events.
 * The public contract and the order of one tick are documented in the header. */

namespace {

constexpr uint8_t kPairInside = 1;   /* overlapping at the end of the step */
constexpr uint8_t kPairCrossing = 2; /* passed through the sensor during the step */

inline uint16_t slot_of(sat_collider2_t id) { return static_cast<uint16_t>(id >> 16); }
inline uint16_t generation_of(sat_collider2_t id) { return static_cast<uint16_t>(id & 0xFFFFu); }
inline sat_collider2_t make_handle(uint16_t slot, uint16_t generation) {
    return (static_cast<uint32_t>(slot) << 16) | generation;
}

using saturn::core::math2d::saturate32;

inline const sat_collider2_slot_t* find_slot(const sat_physics2_world_t* w, sat_collider2_t id) {
    if (!w || id == SAT_COLLIDER2_NONE) return nullptr;
    const uint16_t s = slot_of(id);
    if (s >= w->storage.slot_cap) return nullptr;
    const sat_collider2_slot_t& slot = w->storage.slots[s];
    return slot.alive && slot.generation == generation_of(id) ? &slot : nullptr;
}
inline sat_collider2_slot_t* find_slot(sat_physics2_world_t* w, sat_collider2_t id) {
    return const_cast<sat_collider2_slot_t*>(find_slot(static_cast<const sat_physics2_world_t*>(w), id));
}

inline bool is_moving(const sat_collider2_slot_t& s) { return s.desc.kind != SAT_COLLIDER2_STATIC; }

/* The box a collider swept since the last step: the union of where it was and where it is. */
inline sat_box2_t swept_box(const sat_collider2_slot_t& s) {
    const sat_box2_t& b = s.desc.box;
    const int64_t lox = static_cast<int64_t>(b.center.x < s.previous.x ? b.center.x : s.previous.x) - b.half.x;
    const int64_t hix = static_cast<int64_t>(b.center.x > s.previous.x ? b.center.x : s.previous.x) + b.half.x;
    const int64_t loy = static_cast<int64_t>(b.center.y < s.previous.y ? b.center.y : s.previous.y) - b.half.y;
    const int64_t hiy = static_cast<int64_t>(b.center.y > s.previous.y ? b.center.y : s.previous.y) + b.half.y;
    sat_box2_t out;
    out.center = {saturate32((lox + hix) >> 1), saturate32((loy + hiy) >> 1)};
    out.half = {saturate32((hix - lox + 1) >> 1), saturate32((hiy - loy + 1) >> 1)};
    return out;
}

inline bool valid_desc(const sat_collider2_desc_t& d) {
    return d.kind >= SAT_COLLIDER2_STATIC && d.kind <= SAT_COLLIDER2_SENSOR && d.box.half.x >= 0 &&
           d.box.half.y >= 0 && d.one_way_face <= 3;
}

/* Rebuilds the spatial index from the swept boxes when anything moved. */
bool ensure_index(sat_physics2_world_t* w) {
    if (!w->index_dirty) return !w->index_overflow;
    sat_spatial_t* sp = w->storage.spatial;
    sat_spatial_clear(sp);
    w->index_overflow = 0;
    for (uint16_t i = 0; i < w->storage.slot_cap; ++i) {
        const sat_collider2_slot_t& s = w->storage.slots[i];
        if (!s.alive) continue;
        const sat_box2_t box = swept_box(s);
        if (sat_spatial_insert(sp, i, &box) != SAT_OK) w->index_overflow = 1;
    }
    w->index_dirty = 0;
    return !w->index_overflow;
}

/* Slot indices of every collider whose swept box overlaps `box`, ascending, in
 * w->storage.candidates. */
sat_result_t gather(sat_physics2_world_t* w, const sat_box2_t& box, uint16_t& count) {
    count = 0;
    if (!ensure_index(w)) return SAT_ERR_CAPACITY;
    uint16_t n = 0;
    const sat_result_t r = sat_spatial_query(w->storage.spatial, &box, w->storage.candidates, w->storage.slot_cap, &n);
    if (r != SAT_OK) return r;
    uint16_t* c = w->storage.candidates;
    for (uint16_t i = 1; i < n; ++i) { /* insertion sort: result lists are short */
        const uint16_t v = c[i];
        uint16_t j = i;
        while (j > 0 && c[j - 1] > v) { c[j] = c[j - 1]; --j; }
        c[j] = v;
    }
    count = n;
    return SAT_OK;
}

inline bool matches(const sat_collider2_slot_t& s, uint8_t kind_mask, uint16_t category_mask) {
    return (kind_mask & SAT_COLLIDER2_KIND_BIT(s.desc.kind)) != 0 && (s.desc.category & category_mask) != 0;
}

inline bool key_less(sat_collider2_t sa, sat_collider2_t oa, sat_collider2_t sb, sat_collider2_t ob) {
    return sa != sb ? sa < sb : oa < ob;
}

struct EventSink {
    sat_physics2_event_t* out;
    uint16_t cap;
    sat_physics2_step_result_t* result;
    void push(sat_collider2_t sensor, sat_collider2_t other, uint8_t type, uint8_t flags) {
        if (result->events >= cap || !out) { ++result->events_dropped; return; }
        out[result->events++] = {sensor, other, type, flags, 0};
    }
};

/* Whether `subject` passed through `sensor` between the last step and now, judged in the
 * sensor's frame so a moving sensor works too. The sensor box is shrunk by one raw unit so
 * grazing along an edge does not count, matching the strict overlap rule. */
bool passed_through(const sat_collider2_slot_t& sensor, const sat_collider2_slot_t& subject) {
    if (!is_moving(sensor) && !is_moving(subject)) return false;
    const sat_vec2_t rel = {
        saturate32((static_cast<int64_t>(subject.desc.box.center.x) - subject.previous.x) -
                (static_cast<int64_t>(sensor.desc.box.center.x) - sensor.previous.x)),
        saturate32((static_cast<int64_t>(subject.desc.box.center.y) - subject.previous.y) -
                (static_cast<int64_t>(sensor.desc.box.center.y) - sensor.previous.y))};
    if (rel.x == 0 && rel.y == 0) return false;
    sat_box2_t target = sensor.desc.box;
    target.center = sensor.previous;
    if (target.half.x > 0) --target.half.x;
    if (target.half.y > 0) --target.half.y;
    sat_box2_t moving = subject.desc.box;
    moving.center = subject.previous;
    sat_hit2_t hit;
    return sat_sweep_box2(&moving, &rel, &target, &hit) != 0;
}

} // namespace

extern "C" uint32_t sat_physics2_requirements(uint16_t slot_cap, uint16_t pair_cap) {
    return static_cast<uint32_t>(slot_cap) * sizeof(sat_collider2_slot_t) +
           2u * static_cast<uint32_t>(pair_cap) * sizeof(sat_physics2_pair_t) +
           static_cast<uint32_t>(slot_cap) * sizeof(uint16_t);
}

extern "C" sat_result_t sat_physics2_world_init(sat_physics2_world_t* w, const sat_physics2_storage_t* st) {
    if (!w || !st || !st->slots || st->slot_cap == 0 || st->slot_cap >= SAT_PHYSICS2_NO_SLOT || !st->candidates ||
        !st->spatial || st->spatial->item_cap < st->slot_cap || (st->pair_cap != 0 && (!st->pairs || !st->next_pairs)))
        return SAT_ERR_INVALID_ARG;
    w->storage = *st;
    w->emit_stay = 0;
    for (uint16_t i = 0; i < st->slot_cap; ++i) {
        st->slots[i].alive = 0;
        st->slots[i].generation = 1;
    }
    sat_physics2_world_reset(w);
    return SAT_OK;
}

extern "C" void sat_physics2_world_reset(sat_physics2_world_t* w) {
    if (!w || !w->storage.slots) return;
    for (uint16_t i = 0; i < w->storage.slot_cap; ++i) {
        sat_collider2_slot_t& s = w->storage.slots[i];
        if (s.alive) {
            s.alive = 0;
            if (++s.generation == 0) s.generation = 1;
        }
        s.next_free = i + 1 < w->storage.slot_cap ? static_cast<uint16_t>(i + 1) : SAT_PHYSICS2_NO_SLOT;
    }
    w->free_head = 0;
    w->count = 0;
    w->pair_count = 0;
    w->index_dirty = 1;
    w->index_overflow = 0;
}

extern "C" void sat_physics2_set_emit_stay(sat_physics2_world_t* w, int enabled) {
    if (w) w->emit_stay = enabled ? 1 : 0;
}

extern "C" void sat_collider2_desc_init(sat_collider2_desc_t* d, sat_collider2_kind_t kind, const sat_box2_t* box) {
    if (!d) return;
    *d = {};
    d->kind = kind;
    if (box) d->box = *box;
    d->category = 1;
    d->mask = 0xFFFFu;
    d->launch_scale = SAT_FX16_ONE;
}

extern "C" sat_result_t sat_physics2_add(sat_physics2_world_t* w, const sat_collider2_desc_t* d, sat_collider2_t* out) {
    if (!w || !w->storage.slots || !d || !out || !valid_desc(*d)) return SAT_ERR_INVALID_ARG;
    if (w->free_head == SAT_PHYSICS2_NO_SLOT) return SAT_ERR_CAPACITY;
    const uint16_t index = w->free_head;
    sat_collider2_slot_t& s = w->storage.slots[index];
    w->free_head = s.next_free;
    s.desc = *d;
    s.previous = d->box.center;
    s.alive = 1;
    s.next_free = SAT_PHYSICS2_NO_SLOT;
    ++w->count;
    w->index_dirty = 1;
    *out = make_handle(index, s.generation);
    return SAT_OK;
}

extern "C" sat_result_t sat_physics2_remove(sat_physics2_world_t* w, sat_collider2_t id) {
    sat_collider2_slot_t* s = find_slot(w, id);
    if (!s) return SAT_ERR_NOT_FOUND;
    s->alive = 0;
    if (++s->generation == 0) s->generation = 1;
    s->next_free = w->free_head;
    w->free_head = slot_of(id);
    --w->count;
    w->index_dirty = 1;
    return SAT_OK;
}

extern "C" int sat_physics2_is_valid(const sat_physics2_world_t* w, sat_collider2_t id) {
    return find_slot(w, id) != nullptr;
}

extern "C" sat_result_t sat_physics2_get(const sat_physics2_world_t* w, sat_collider2_t id, sat_collider2_desc_t* out) {
    const sat_collider2_slot_t* s = find_slot(w, id);
    if (!s) return SAT_ERR_NOT_FOUND;
    if (!out) return SAT_ERR_INVALID_ARG;
    *out = s->desc;
    return SAT_OK;
}

extern "C" sat_result_t sat_physics2_set_center(sat_physics2_world_t* w, sat_collider2_t id, sat_vec2_t center) {
    sat_collider2_slot_t* s = find_slot(w, id);
    if (!s) return SAT_ERR_NOT_FOUND;
    if (!is_moving(*s)) return SAT_ERR_INVALID_ARG;
    s->desc.box.center = center;
    w->index_dirty = 1;
    return SAT_OK;
}

extern "C" sat_result_t sat_physics2_set_box(sat_physics2_world_t* w, sat_collider2_t id, const sat_box2_t* box) {
    sat_collider2_slot_t* s = find_slot(w, id);
    if (!s) return SAT_ERR_NOT_FOUND;
    if (!box || box->half.x < 0 || box->half.y < 0 || !is_moving(*s)) return SAT_ERR_INVALID_ARG;
    s->desc.box = *box;
    w->index_dirty = 1;
    return SAT_OK;
}

extern "C" sat_result_t sat_physics2_delta(const sat_physics2_world_t* w, sat_collider2_t id, sat_vec2_t* out) {
    const sat_collider2_slot_t* s = find_slot(w, id);
    if (!s) return SAT_ERR_NOT_FOUND;
    if (!out) return SAT_ERR_INVALID_ARG;
    *out = {saturate32(static_cast<int64_t>(s->desc.box.center.x) - s->previous.x),
            saturate32(static_cast<int64_t>(s->desc.box.center.y) - s->previous.y)};
    return SAT_OK;
}

extern "C" sat_result_t sat_physics2_query(sat_physics2_world_t* w, const sat_box2_t* box, uint8_t kind_mask,
                                           uint16_t category_mask, sat_collider2_t* out, uint16_t cap, uint16_t* count) {
    if (!w || !w->storage.slots || !box || box->half.x < 0 || box->half.y < 0 || !count || (cap != 0 && !out))
        return SAT_ERR_INVALID_ARG;
    *count = 0;
    uint16_t n = 0;
    const sat_result_t r = gather(w, *box, n);
    if (r != SAT_OK) return r;
    sat_result_t result = SAT_OK;
    for (uint16_t i = 0; i < n; ++i) {
        const uint16_t index = w->storage.candidates[i];
        const sat_collider2_slot_t& s = w->storage.slots[index];
        if (!matches(s, kind_mask, category_mask) || !saturn::core::collide2d::box_overlap(*box, s.desc.box)) continue;
        if (*count >= cap) { result = SAT_ERR_CAPACITY; continue; }
        out[(*count)++] = make_handle(index, s.generation);
    }
    return result;
}

extern "C" sat_result_t sat_physics2_find_support(sat_physics2_world_t* w, const sat_physics2_support_query_t* q,
                                                  sat_physics2_support_t* out) {
    if (!w || !w->storage.slots || !q || !out || q->down_quadrant > 3 || q->half_width < 0) return SAT_ERR_INVALID_ARG;
    const int axis = q->down_quadrant & 1;
    const int sign = q->down_quadrant < 2 ? 1 : -1;
    const int across = axis ^ 1;
    const int64_t origin_axis = axis == 0 ? q->origin.x : q->origin.y;
    const int64_t origin_across = across == 0 ? q->origin.x : q->origin.y;

    /* the probe: from `embed` px behind the origin to `reach` px in front of it */
    const int64_t a = origin_axis - static_cast<int64_t>(sign) * q->embed * SAT_FX16_ONE;
    const int64_t b = origin_axis + static_cast<int64_t>(sign) * q->reach * SAT_FX16_ONE;
    const int64_t lo = a < b ? a : b;
    const int64_t hi = a < b ? b : a;
    sat_box2_t probe;
    const sat_vec2_t along = {saturate32((lo + hi) >> 1), saturate32((hi - lo + 1) >> 1)}; /* centre, half */
    const sat_vec2_t cross = {saturate32(origin_across), q->half_width};
    probe.center = axis == 0 ? sat_vec2_t{along.x, cross.x} : sat_vec2_t{cross.x, along.x};
    probe.half = axis == 0 ? sat_vec2_t{along.y, cross.y} : sat_vec2_t{cross.y, along.y};

    uint16_t n = 0;
    const sat_result_t r = gather(w, probe, n);
    if (r != SAT_OK) return r;

    const uint8_t face = static_cast<uint8_t>((q->down_quadrant + 2) & 3); /* outward normal of the face we land on */
    bool found = false;
    int64_t best = 0;
    uint16_t best_slot = 0;
    for (uint16_t i = 0; i < n; ++i) {
        const uint16_t index = w->storage.candidates[i];
        const sat_collider2_slot_t& s = w->storage.slots[index];
        if (!matches(s, SAT_COLLIDER2_SOLID_KINDS, q->category_mask) || !saturn::core::collide2d::box_overlap(probe, s.desc.box)) continue;
        if ((s.desc.flags & SAT_COLLIDER2_ONE_WAY) && s.desc.one_way_face != face) continue;
        const int64_t centre = axis == 0 ? s.desc.box.center.x : s.desc.box.center.y;
        const int64_t half = axis == 0 ? s.desc.box.half.x : s.desc.box.half.y;
        const int64_t distance = sign * ((centre - sign * half) - origin_axis); /* to the near face */
        if (distance < -static_cast<int64_t>(q->embed) * SAT_FX16_ONE || distance >= static_cast<int64_t>(q->reach) * SAT_FX16_ONE) continue;
        if (!found || distance < best) { /* ascending slots: the lowest handle wins a tie */
            found = true;
            best = distance;
            best_slot = index;
        }
    }
    if (!found) return SAT_ERR_NOT_FOUND;

    const sat_collider2_slot_t& s = w->storage.slots[best_slot];
    out->id = make_handle(best_slot, s.generation);
    out->distance = saturate32(best);
    sat_vec2_t point = q->origin;
    (axis == 0 ? point.x : point.y) = saturate32(origin_axis + sign * best);
    out->point = point;
    out->normal = {face == 0 ? SAT_FX16_ONE : (face == 2 ? -SAT_FX16_ONE : 0),
                   face == 1 ? SAT_FX16_ONE : (face == 3 ? -SAT_FX16_ONE : 0)};
    return sat_physics2_delta(w, out->id, &out->delta);
}

extern "C" sat_result_t sat_physics2_carry(const sat_physics2_world_t* w, sat_collider2_t id, sat_vec2_t* position) {
    sat_vec2_t d;
    const sat_result_t r = sat_physics2_delta(w, id, &d);
    if (r != SAT_OK) return r;
    if (!position) return SAT_ERR_INVALID_ARG;
    position->x = saturate32(static_cast<int64_t>(position->x) + d.x);
    position->y = saturate32(static_cast<int64_t>(position->y) + d.y);
    return SAT_OK;
}

extern "C" sat_result_t sat_physics2_launch_velocity(const sat_physics2_world_t* w, sat_collider2_t id, sat_vec2_t* out) {
    sat_vec2_t d;
    const sat_result_t r = sat_physics2_delta(w, id, &d);
    if (r != SAT_OK) return r;
    if (!out) return SAT_ERR_INVALID_ARG;
    const sat_collider2_slot_t* s = find_slot(w, id);
    if (!s) return SAT_ERR_NOT_FOUND;
    const int64_t scale = s->desc.launch_scale;
    *out = {saturate32((static_cast<int64_t>(d.x) * scale) >> 16), saturate32((static_cast<int64_t>(d.y) * scale) >> 16)};
    return SAT_OK;
}

extern "C" sat_result_t sat_physics2_step(sat_physics2_world_t* w, sat_physics2_event_t* events, uint16_t cap,
                                          sat_physics2_step_result_t* result) {
    if (!w || !w->storage.slots || (cap != 0 && !events)) return SAT_ERR_INVALID_ARG;
    sat_physics2_step_result_t local = {};
    sat_physics2_step_result_t& res = result ? *result : local;
    res = {};
    EventSink sink = {events, cap, &res};
    sat_physics2_storage_t& st = w->storage;

    if (!ensure_index(w)) return SAT_ERR_CAPACITY;

    /* 1. the pairs that exist now: sensors in handle order, solid colliders in handle order */
    uint16_t next_count = 0;
    for (uint16_t si = 0; si < st.slot_cap; ++si) {
        const sat_collider2_slot_t& sensor = st.slots[si];
        if (!sensor.alive || sensor.desc.kind != SAT_COLLIDER2_SENSOR) continue;
        const sat_box2_t reach = swept_box(sensor);
        uint16_t n = 0;
        if (gather(w, reach, n) != SAT_OK) continue; /* cannot fail beyond the index check above */
        for (uint16_t i = 0; i < n; ++i) {
            const uint16_t oi = st.candidates[i];
            const sat_collider2_slot_t& other = st.slots[oi];
            if (!matches(other, SAT_COLLIDER2_SOLID_KINDS, sensor.desc.mask)) continue;
            uint8_t state = 0;
            if (saturn::core::collide2d::box_overlap(sensor.desc.box, other.desc.box)) state = kPairInside;
            else if (passed_through(sensor, other)) state = kPairCrossing;
            if (state == 0) continue;
            if (next_count >= st.pair_cap) { ++res.pairs_dropped; continue; }
            st.next_pairs[next_count++] = {make_handle(si, sensor.generation), make_handle(oi, other.generation), state, {0, 0, 0}};
        }
    }

    /* 2. merge the old and new pair lists (both sorted by handle) into events */
    uint16_t oi = 0, ni = 0;
    while (oi < w->pair_count || ni < next_count) {
        const bool have_old = oi < w->pair_count;
        const bool have_new = ni < next_count;
        const sat_physics2_pair_t* o = have_old ? &st.pairs[oi] : nullptr;
        const sat_physics2_pair_t* nw = have_new ? &st.next_pairs[ni] : nullptr;
        int order; /* <0 old first, 0 same pair, >0 new first */
        if (!have_new) order = -1;
        else if (!have_old) order = 1;
        else if (key_less(o->sensor, o->other, nw->sensor, nw->other)) order = -1;
        else if (key_less(nw->sensor, nw->other, o->sensor, o->other)) order = 1;
        else order = 0;

        if (order < 0) {
            const bool gone = !find_slot(w, o->sensor) || !find_slot(w, o->other);
            sink.push(o->sensor, o->other, SAT_PHYSICS2_EVENT_LEAVE, gone ? SAT_PHYSICS2_EVENT_REMOVED : 0);
            ++oi;
        } else if (order > 0) {
            if (nw->state & kPairInside) {
                sink.push(nw->sensor, nw->other, SAT_PHYSICS2_EVENT_ENTER, 0);
            } else {
                sink.push(nw->sensor, nw->other, SAT_PHYSICS2_EVENT_ENTER, SAT_PHYSICS2_EVENT_CROSSING);
                sink.push(nw->sensor, nw->other, SAT_PHYSICS2_EVENT_LEAVE, SAT_PHYSICS2_EVENT_CROSSING);
            }
            ++ni;
        } else {
            if (nw->state & kPairInside) {
                if (w->emit_stay) sink.push(nw->sensor, nw->other, SAT_PHYSICS2_EVENT_STAY, 0);
            } else {
                sink.push(o->sensor, o->other, SAT_PHYSICS2_EVENT_LEAVE, 0);
            }
            ++oi;
            ++ni;
        }
    }

    /* 3. keep only the pairs that overlap, make them the current list, commit the tick */
    uint16_t kept = 0;
    for (uint16_t i = 0; i < next_count; ++i)
        if (st.next_pairs[i].state & kPairInside) st.next_pairs[kept++] = st.next_pairs[i];
    sat_physics2_pair_t* swap = st.pairs;
    st.pairs = st.next_pairs;
    st.next_pairs = swap;
    w->pair_count = kept;

    for (uint16_t i = 0; i < st.slot_cap; ++i)
        if (st.slots[i].alive) st.slots[i].previous = st.slots[i].desc.box.center;
    w->index_dirty = 1;

    return res.events_dropped != 0 || res.pairs_dropped != 0 ? SAT_ERR_CAPACITY : SAT_OK;
}
