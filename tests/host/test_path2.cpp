#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "saturn/path2.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); std::exit(1); } } while (0)

static const double kPi = 3.14159265358979323846;

static sat_fx16_t F(double px) { return static_cast<sat_fx16_t>(std::lround(px * SAT_FX16_ONE)); }
static sat_vec2_t V(double x, double y) { return {F(x), F(y)}; }
static double D(sat_fx16_t v) { return v / 65536.0; }
static bool near(sat_fx16_t raw, double expect, double tol = 0.05) { return std::fabs(D(raw) - expect) <= tol; }
static bool at(sat_vec2_t p, double x, double y, double tol = 0.05) { return near(p.x, x, tol) && near(p.y, y, tol); }
static bool unit(sat_vec2_t v, double tol = 0.002) {
    return std::fabs(std::hypot(D(v.x), D(v.y)) - 1.0) <= tol;
}

static sat_path2_sample_t sample(const sat_path2_t& p, double distance) {
    sat_path2_sample_t s = {};
    OK(sat_path2_sample(&p, F(distance), &s) == SAT_OK);
    return s;
}

static void line() {
    sat_path2_t p;
    OK(sat_path2_init_line(&p, V(0, 0), V(30, 40)) == SAT_OK);
    OK(near(sat_path2_length(&p), 50.0, 0.01) && !sat_path2_is_closed(&p));
    sat_path2_sample_t s = sample(p, 0);
    OK(at(s.position, 0, 0) && near(s.tangent.x, 0.6, 0.002) && near(s.tangent.y, 0.8, 0.002));
    OK(near(s.normal.x, 0.8, 0.002) && near(s.normal.y, -0.6, 0.002)); /* left of travel on screen */
    s = sample(p, 25);
    OK(at(s.position, 15, 20) && near(s.distance, 25.0, 0.001));
    s = sample(p, 50);
    OK(at(s.position, 30, 40));
    /* out of range clamps on an open path */
    OK(at(sample(p, -10).position, 0, 0) && near(sample(p, -10).distance, 0.0));
    OK(at(sample(p, 90).position, 30, 40) && near(sample(p, 90).distance, 50.0, 0.01));
    /* a path along +X has its normal up the screen, like Terrain2 */
    sat_path2_t h;
    OK(sat_path2_init_line(&h, V(0, 0), V(10, 0)) == SAT_OK);
    s = sample(h, 5);
    OK(near(s.tangent.x, 1.0, 0.001) && near(s.normal.y, -1.0, 0.001));
    /* a degenerate line is refused */
    OK(sat_path2_init_line(&p, V(5, 5), V(5, 5)) == SAT_ERR_INVALID_ARG);
    OK(sat_path2_init_line(nullptr, V(0, 0), V(1, 1)) == SAT_ERR_INVALID_ARG);
}

static void polyline() {
    const sat_vec2_t pts[4] = {V(0, 0), V(40, 0), V(40, 30), V(0, 30)};
    sat_fx16_t cum[8];
    OK(sat_path2_polyline_entries(4, 0) == 4 && sat_path2_polyline_entries(4, 1) == 5);
    sat_path2_t p;
    OK(sat_path2_init_polyline(&p, pts, 4, 0, cum, 3) == SAT_ERR_INVALID_ARG); /* storage too small */
    OK(sat_path2_init_polyline(&p, pts, 1, 0, cum, 8) == SAT_ERR_INVALID_ARG);
    OK(sat_path2_init_polyline(&p, pts, 4, 0, cum, 8) == SAT_OK);
    OK(near(sat_path2_length(&p), 110.0, 0.01));
    OK(near(cum[1], 40.0, 0.01) && near(cum[2], 70.0, 0.01) && near(cum[3], 110.0, 0.01));

    /* the vertices land exactly where they should and each segment has its own tangent */
    OK(at(sample(p, 0).position, 0, 0));
    OK(at(sample(p, 40).position, 40, 0));
    OK(near(sample(p, 40).tangent.y, 1.0, 0.002) && near(sample(p, 39.9).tangent.x, 1.0, 0.002)); /* turns at the vertex */
    OK(at(sample(p, 55).position, 40, 15));
    OK(at(sample(p, 70).position, 40, 30));
    OK(near(sample(p, 90).tangent.x, -1.0, 0.002) && at(sample(p, 90).position, 20, 30));
    OK(at(sample(p, 110).position, 0, 30) && near(sample(p, 110).tangent.x, -1.0, 0.002));
    OK(at(sample(p, 500).position, 0, 30)); /* clamped */

    /* a closed polyline gets the last edge back to the start and wraps */
    sat_fx16_t cumc[8];
    sat_path2_t c;
    OK(sat_path2_init_polyline(&c, pts, 4, 1, cumc, 5) == SAT_OK);
    OK(near(sat_path2_length(&c), 140.0, 0.01) && sat_path2_is_closed(&c));
    OK(at(sample(c, 125).position, 0, 15) && near(sample(c, 125).tangent.y, -1.0, 0.002));
    OK(at(sample(c, 145).position, 5, 0)); /* wrapped */
    OK(at(sample(c, -5).position, 0, 5));

    /* repeated and zero-length vertices do not break the walk */
    const sat_vec2_t dup[5] = {V(0, 0), V(0, 0), V(10, 0), V(10, 0), V(10, 10)};
    sat_fx16_t cumd[8];
    sat_path2_t d;
    OK(sat_path2_init_polyline(&d, dup, 5, 0, cumd, 8) == SAT_OK && near(sat_path2_length(&d), 20.0, 0.01));
    OK(at(sample(d, 0).position, 0, 0) && near(sample(d, 0).tangent.x, 1.0, 0.002));
    OK(at(sample(d, 10).position, 10, 0));
    OK(at(sample(d, 15).position, 10, 5) && near(sample(d, 15).tangent.y, 1.0, 0.002));
    OK(at(sample(d, 20).position, 10, 10) && near(sample(d, 20).tangent.y, 1.0, 0.002));

    /* all vertices equal: nothing to walk */
    const sat_vec2_t flat[3] = {V(1, 1), V(1, 1), V(1, 1)};
    sat_fx16_t cumf[4];
    sat_path2_t f;
    OK(sat_path2_init_polyline(&f, flat, 3, 0, cumf, 4) == SAT_ERR_INVALID_ARG);
}

static void arcs_and_circles() {
    sat_path2_t a;
    /* a quarter turn clockwise on screen: from +X to +Y about the origin */
    OK(sat_path2_init_arc(&a, V(0, 0), F(100), 0, 16384) == SAT_OK);
    OK(near(sat_path2_length(&a), 100.0 * kPi / 2, 0.05) && !sat_path2_is_closed(&a));
    sat_path2_sample_t s = sample(a, 0);
    OK(at(s.position, 100, 0) && near(s.tangent.x, 0.0, 0.002) && near(s.tangent.y, 1.0, 0.002));
    OK(unit(s.tangent) && unit(s.normal));
    s = sample(a, D(sat_path2_length(&a)) / 2);
    OK(at(s.position, 100 * std::cos(kPi / 4), 100 * std::sin(kPi / 4), 0.06));
    s = sample(a, D(sat_path2_length(&a)));
    OK(at(s.position, 0, 100, 0.06) && near(s.tangent.x, -1.0, 0.002));

    /* the other way round: a negative sweep travels towards -Y */
    sat_path2_t b;
    OK(sat_path2_init_arc(&b, V(10, 20), F(50), 0, -16384) == SAT_OK);
    s = sample(b, 0);
    OK(at(s.position, 60, 20) && near(s.tangent.y, -1.0, 0.002));
    s = sample(b, D(sat_path2_length(&b)));
    OK(at(s.position, 10, -30, 0.06) && near(s.tangent.x, -1.0, 0.002));

    /* a start angle other than 0 */
    sat_path2_t c;
    OK(sat_path2_init_arc(&c, V(0, 0), F(100), 32768, 8192) == SAT_OK); /* from -X, an eighth of a turn */
    s = sample(c, 0);
    OK(at(s.position, -100, 0, 0.06));
    s = sample(c, D(sat_path2_length(&c)));
    OK(at(s.position, -100 * std::cos(kPi / 4), -100 * std::sin(kPi / 4), 0.08));

    /* a circle wraps and is closed */
    sat_path2_t o;
    OK(sat_path2_init_circle(&o, V(0, 0), F(64)) == SAT_OK && sat_path2_is_closed(&o));
    const double len = D(sat_path2_length(&o));
    OK(std::fabs(len - 2 * kPi * 64) < 0.05);
    OK(at(sample(o, len / 4).position, 0, 64, 0.06));
    OK(at(sample(o, len * 1.25).position, 0, 64, 0.06));
    OK(at(sample(o, -len / 4).position, 0, -64, 0.06));
    OK(at(sample(o, 0).position, 64, 0) && at(sample(o, len).position, 64, 0, 0.06));

    /* a full-turn arc is closed too, with its own start and direction */
    sat_path2_t full;
    OK(sat_path2_init_arc(&full, V(0, 0), F(10), 16384, -65536) == SAT_OK && sat_path2_is_closed(&full));
    OK(at(sample(full, 0).position, 0, 10) && near(sample(full, 0).tangent.x, 1.0, 0.002));

    /* every sampled tangent is a unit vector perpendicular to the radius */
    for (int i = 0; i <= 40; ++i) {
        s = sample(o, len * i / 40.0);
        OK(unit(s.tangent) && unit(s.normal));
        const double dot = D(s.tangent.x) * D(s.position.x) + D(s.tangent.y) * D(s.position.y);
        OK(std::fabs(dot) < 0.2); /* radius is 64 */
    }

    /* invalid shapes */
    OK(sat_path2_init_arc(&a, V(0, 0), 0, 0, 100) == SAT_ERR_INVALID_ARG);
    OK(sat_path2_init_arc(&a, V(0, 0), F(10), 0, 0) == SAT_ERR_INVALID_ARG);
    OK(sat_path2_init_arc(&a, V(0, 0), F(10), 0, 65537) == SAT_ERR_INVALID_ARG);
    OK(sat_path2_init_arc(&a, V(0, 0), F(-10), 0, 100) == SAT_ERR_INVALID_ARG);
    OK(sat_path2_init_circle(&a, V(0, 0), 0) == SAT_ERR_INVALID_ARG);
    OK(sat_path2_init_arc(&a, V(0, 0), F(30000), 0, 65536) == SAT_ERR_INVALID_ARG); /* longer than 32767 px */
}

static void beziers() {
    sat_path2_t q;
    OK(sat_path2_init_quadratic(&q, V(0, 0), V(50, 100), V(100, 0)) == SAT_OK);
    /* a symmetric parabola: length 147.89 (arc of y = 100 - (x-50)^2/25 .. analytic) */
    const double qlen = D(sat_path2_length(&q));
    OK(qlen > 147.0 && qlen < 148.5);
    OK(at(sample(q, 0).position, 0, 0) && at(sample(q, qlen).position, 100, 0));
    OK(at(sample(q, qlen / 2).position, 50, 50, 0.3)); /* the apex of a quadratic with control (50, 100) */
    OK(unit(sample(q, 20).tangent) && unit(sample(q, 20).normal));
    OK(near(sample(q, 0).tangent.x, 50 / std::hypot(50.0, 100.0), 0.002));

    /* a straight quadratic is its chord */
    sat_path2_t sq;
    OK(sat_path2_init_quadratic(&sq, V(0, 0), V(10, 10), V(20, 20)) == SAT_OK);
    OK(near(sat_path2_length(&sq), 20 * std::sqrt(2.0), 0.02));
    OK(at(sample(sq, D(sat_path2_length(&sq)) / 2).position, 10, 10, 0.2));

    /* a cubic that approximates a quarter circle of radius 100 */
    const double k = 0.5522847498;
    sat_path2_t c;
    OK(sat_path2_init_cubic(&c, V(100, 0), V(100, 100 * k), V(100 * k, 100), V(0, 100)) == SAT_OK);
    const double clen = D(sat_path2_length(&c));
    OK(std::fabs(clen - 100 * kPi / 2) < 0.4);
    OK(at(sample(c, 0).position, 100, 0) && at(sample(c, clen).position, 0, 100));
    OK(near(sample(c, 0).tangent.y, 1.0, 0.002) && near(sample(c, clen).tangent.x, -1.0, 0.002));
    for (int i = 0; i <= 20; ++i) {
        const sat_path2_sample_t s = sample(c, clen * i / 20.0);
        OK(unit(s.tangent) && unit(s.normal));
        OK(std::fabs(std::hypot(D(s.position.x), D(s.position.y)) - 100.0) < 0.4); /* stays on the circle */
    }

    /* a cusp (control point on the end point) still has a tangent at the end */
    sat_path2_t cusp;
    OK(sat_path2_init_cubic(&cusp, V(0, 0), V(0, 0), V(40, 0), V(40, 40)) == SAT_OK);
    OK(unit(sample(cusp, 0).tangent) && near(sample(cusp, 0).tangent.x, 1.0, 0.05));

    /* degenerate: every control point equal */
    OK(sat_path2_init_cubic(&cusp, V(3, 3), V(3, 3), V(3, 3), V(3, 3)) == SAT_ERR_INVALID_ARG);
    OK(sat_path2_init_quadratic(&cusp, V(3, 3), V(3, 3), V(3, 3)) == SAT_ERR_INVALID_ARG);
}

static void arc_length_tables() {
    /* a lopsided cubic: the middle of the parameter range is nowhere near the middle of the curve */
    sat_path2_t c;
    OK(sat_path2_init_cubic(&c, V(0, 0), V(2, 0), V(4, 0), V(200, 0)) == SAT_OK);
    const double len = D(sat_path2_length(&c));
    OK(std::fabs(len - 200.0) < 0.5);
    const sat_path2_sample_t half_uniform = sample(c, len / 2);

    sat_fx16_t table[65];
    OK(sat_path2_build_table(&c, table, 1) == SAT_ERR_INVALID_ARG);
    OK(sat_path2_build_table(&c, table, SAT_PATH2_TABLE_MAX + 1) == SAT_ERR_INVALID_ARG);
    OK(sat_path2_table_requirements(65) == 65 * sizeof(sat_fx16_t));
    OK(sat_path2_build_table(&c, table, 65) == SAT_OK);
    OK(table[0] == 0 && table[64] == sat_path2_length(&c));
    for (int i = 1; i < 65; ++i) OK(table[i] >= table[i - 1]);

    /* with the table, equal steps of distance are equal steps in space (straight line, so x is distance) */
    double worst = 0;
    for (int i = 0; i <= 20; ++i) {
        const double s = D(sat_path2_length(&c)) * i / 20.0;
        worst = std::fmax(worst, std::fabs(D(sample(c, s).position.x) - s));
    }
    OK(worst < 1.0);
    /* without it the same call is far from the true half-way point */
    OK(std::fabs(D(half_uniform.position.x) - len / 2) > 20.0);

    /* an offline table of the same shape is accepted; a malformed one is not */
    sat_path2_t d;
    OK(sat_path2_init_cubic(&d, V(0, 0), V(2, 0), V(4, 0), V(200, 0)) == SAT_OK);
    OK(sat_path2_attach_table(&d, table, 65) == SAT_OK && d.length == table[64]);
    OK(near(sample(d, 100).position.x, 100.0, 1.0));
    sat_fx16_t bad[4] = {0, F(10), F(5), F(20)};
    OK(sat_path2_attach_table(&d, bad, 4) == SAT_ERR_INVALID_ARG); /* decreasing */
    bad[0] = F(1);
    bad[2] = F(15);
    OK(sat_path2_attach_table(&d, bad, 4) == SAT_ERR_INVALID_ARG); /* does not start at 0 */
    bad[0] = 0;
    bad[3] = 0;
    OK(sat_path2_attach_table(&d, bad, 4) == SAT_ERR_INVALID_ARG); /* no length */
    OK(sat_path2_attach_table(&d, table, 1) == SAT_ERR_INVALID_ARG);

    /* tables are for curves only */
    sat_path2_t line;
    OK(sat_path2_init_line(&line, V(0, 0), V(5, 5)) == SAT_OK);
    OK(sat_path2_build_table(&line, table, 8) == SAT_ERR_INVALID_ARG);
    OK(sat_path2_attach_table(&line, table, 8) == SAT_ERR_INVALID_ARG);
}

static void advance_clamp_and_wrap() {
    const sat_vec2_t pts[3] = {V(0, 0), V(50, 0), V(50, 50)};
    sat_fx16_t cum[4];
    sat_path2_t p;
    OK(sat_path2_init_polyline(&p, pts, 3, 0, cum, 4) == SAT_OK); /* length 100 */
    sat_fx16_t out;
    uint8_t flags;

    OK(sat_path2_advance(&p, F(10), F(30), SAT_PATH2_CLAMP, &out, &flags) == SAT_OK && out == F(40) && flags == 0);
    OK(sat_path2_advance(&p, F(90), F(30), SAT_PATH2_CLAMP, &out, &flags) == SAT_OK && out == F(100) && flags == SAT_PATH2_HIT_END);
    OK(sat_path2_advance(&p, F(90), F(10), SAT_PATH2_CLAMP, &out, &flags) == SAT_OK && out == F(100) && flags == SAT_PATH2_HIT_END); /* lands exactly */
    OK(sat_path2_advance(&p, F(10), F(-30), SAT_PATH2_CLAMP, &out, &flags) == SAT_OK && out == 0 && flags == SAT_PATH2_HIT_START);
    OK(sat_path2_advance(&p, F(30), F(-30), SAT_PATH2_CLAMP, &out, &flags) == SAT_OK && out == 0 && flags == SAT_PATH2_HIT_START);
    OK(sat_path2_advance(&p, F(10), 0, SAT_PATH2_CLAMP, &out, nullptr) == SAT_OK && out == F(10));

    /* wrapping works on an open path too, and handles several laps */
    OK(sat_path2_advance(&p, F(90), F(30), SAT_PATH2_WRAP, &out, &flags) == SAT_OK && out == F(20) && flags == SAT_PATH2_WRAPPED);
    OK(sat_path2_advance(&p, F(10), F(-30), SAT_PATH2_WRAP, &out, &flags) == SAT_OK && out == F(80) && flags == SAT_PATH2_WRAPPED);
    OK(sat_path2_advance(&p, F(10), F(250), SAT_PATH2_WRAP, &out, &flags) == SAT_OK && out == F(60) && flags == SAT_PATH2_WRAPPED);
    OK(sat_path2_advance(&p, F(10), F(100), SAT_PATH2_WRAP, &out, &flags) == SAT_OK && out == F(10) && flags == SAT_PATH2_WRAPPED);
    OK(sat_path2_advance(&p, F(10), F(20), SAT_PATH2_WRAP, &out, &flags) == SAT_OK && out == F(30) && flags == 0);

    /* walking a closed circle in small steps never leaves [0, length) and covers whole laps */
    sat_path2_t o;
    OK(sat_path2_init_circle(&o, V(0, 0), F(20)) == SAT_OK);
    sat_fx16_t d = 0;
    int wraps = 0;
    for (int i = 0; i < 500; ++i) {
        OK(sat_path2_advance(&o, d, F(1.25), SAT_PATH2_WRAP, &d, &flags) == SAT_OK);
        OK(d >= 0 && d < sat_path2_length(&o));
        if (flags & SAT_PATH2_WRAPPED) ++wraps;
    }
    OK(wraps == 4 || wraps == 5); /* 625 px of a 125.7 px circle */

    OK(sat_path2_advance(&p, 0, 0, static_cast<sat_path2_edge_t>(7), &out, &flags) == SAT_ERR_INVALID_ARG);
    OK(sat_path2_advance(&p, 0, 0, SAT_PATH2_CLAMP, nullptr, &flags) == SAT_ERR_INVALID_ARG);
    sat_path2_t blank = {};
    OK(sat_path2_advance(&blank, 0, 0, SAT_PATH2_CLAMP, &out, &flags) == SAT_ERR_INVALID_ARG);
    sat_path2_sample_t s;
    OK(sat_path2_sample(&blank, 0, &s) == SAT_ERR_INVALID_ARG && sat_path2_sample(&p, 0, nullptr) == SAT_ERR_INVALID_ARG);
    OK(sat_path2_length(&blank) == 0 && !sat_path2_is_closed(&blank) && sat_path2_length(nullptr) == 0);
}

static sat_path2_nearest_t nearest(const sat_path2_t& p, double x, double y) {
    sat_path2_nearest_t n = {};
    OK(sat_path2_nearest(&p, V(x, y), &n) == SAT_OK);
    return n;
}

static void nearest_point() {
    sat_path2_t l;
    OK(sat_path2_init_line(&l, V(0, 0), V(100, 0)) == SAT_OK);
    sat_path2_nearest_t n = nearest(l, 30, 12);
    OK(near(n.sample.distance, 30.0, 0.01) && at(n.sample.position, 30, 0) && near(n.gap, 12.0, 0.01));
    n = nearest(l, -20, 5); /* before the start: the start */
    OK(near(n.sample.distance, 0.0) && near(n.gap, std::hypot(20.0, 5.0), 0.02));
    n = nearest(l, 140, -5);
    OK(near(n.sample.distance, 100.0, 0.01) && at(n.sample.position, 100, 0));

    const sat_vec2_t pts[4] = {V(0, 0), V(50, 0), V(50, 50), V(0, 50)};
    sat_fx16_t cum[5];
    sat_path2_t pl;
    OK(sat_path2_init_polyline(&pl, pts, 4, 1, cum, 5) == SAT_OK);
    n = nearest(pl, 60, 20);
    OK(near(n.sample.distance, 70.0, 0.01) && at(n.sample.position, 50, 20) && near(n.gap, 10.0, 0.01));
    n = nearest(pl, 25, 25); /* centre: four equal candidates, the smallest distance wins */
    OK(near(n.sample.distance, 25.0, 0.01) && near(n.gap, 25.0, 0.02));
    n = nearest(pl, -5, 25); /* beside the closing edge */
    OK(near(n.sample.distance, 175.0, 0.01) && at(n.sample.position, 0, 25));

    sat_path2_t a;
    OK(sat_path2_init_arc(&a, V(0, 0), F(100), 0, 16384) == SAT_OK); /* +X to +Y */
    n = nearest(a, 200, 200);
    OK(at(n.sample.position, 100 / std::sqrt(2.0), 100 / std::sqrt(2.0), 0.4) && near(n.gap, 200 * std::sqrt(2.0) - 100, 0.5));
    n = nearest(a, 50, 50); /* inside the circle, on the diagonal */
    OK(at(n.sample.position, 100 / std::sqrt(2.0), 100 / std::sqrt(2.0), 0.4));
    n = nearest(a, 200, -30); /* outside the swept range: clamps to the start */
    OK(near(n.sample.distance, 0.0, 0.5) && at(n.sample.position, 100, 0, 0.5));
    n = nearest(a, -30, 200);
    OK(near(n.sample.distance, D(sat_path2_length(&a)), 0.5) && at(n.sample.position, 0, 100, 0.5));
    n = nearest(a, 0, 0); /* the centre: any point is as near as any other; the result is valid */
    OK(n.gap > F(99) && n.gap < F(101));

    sat_path2_t o;
    OK(sat_path2_init_circle(&o, V(0, 0), F(50)) == SAT_OK);
    n = nearest(o, -80, 0);
    OK(at(n.sample.position, -50, 0, 0.4) && near(n.gap, 30.0, 0.4));
    n = nearest(o, 3, -90);
    OK(at(n.sample.position, 50 * 3 / std::hypot(3.0, 90.0), -50 * 90 / std::hypot(3.0, 90.0), 0.4));

    sat_path2_t c;
    OK(sat_path2_init_cubic(&c, V(100, 0), V(100, 55.2285), V(55.2285, 100), V(0, 100)) == SAT_OK);
    sat_fx16_t table[33];
    OK(sat_path2_build_table(&c, table, 33) == SAT_OK);
    for (int deg = 5; deg < 90; deg += 10) {
        const double r = deg * kPi / 180;
        n = nearest(c, 160 * std::cos(r), 160 * std::sin(r));
        OK(at(n.sample.position, 100 * std::cos(r), 100 * std::sin(r), 0.7));
        OK(near(n.gap, 60.0, 0.7));
        /* a point on the curve is at distance ~0 and the distance matches its sample */
        const sat_path2_sample_t s = sample(c, D(sat_path2_length(&c)) * deg / 90.0);
        const sat_path2_nearest_t on = nearest(c, D(s.position.x), D(s.position.y));
        OK(D(on.gap) < 0.1 && std::fabs(D(on.sample.distance) - D(s.distance)) < 0.3);
    }
    n = nearest(c, 300, -10); /* beyond the start */
    OK(at(n.sample.position, 100, 0, 0.3));

    sat_path2_t blank = {};
    sat_path2_nearest_t nn;
    OK(sat_path2_nearest(&blank, V(0, 0), &nn) == SAT_ERR_INVALID_ARG && sat_path2_nearest(&l, V(0, 0), nullptr) == SAT_ERR_INVALID_ARG);
}

static void repeatability() {
    sat_path2_t c;
    OK(sat_path2_init_cubic(&c, V(0, 0), V(30, 80), V(90, -40), V(120, 20)) == SAT_OK);
    sat_fx16_t table[65];
    OK(sat_path2_build_table(&c, table, 65) == SAT_OK);
    sat_path2_t copy = c; /* a path value can be copied */
    std::vector<sat_path2_sample_t> a, b;
    for (int i = 0; i <= 50; ++i) {
        a.push_back(sample(c, D(sat_path2_length(&c)) * i / 50.0));
        b.push_back(sample(copy, D(sat_path2_length(&copy)) * i / 50.0));
    }
    for (size_t i = 0; i < a.size(); ++i)
        OK(a[i].position.x == b[i].position.x && a[i].position.y == b[i].position.y && a[i].tangent.x == b[i].tangent.x &&
           a[i].distance == b[i].distance);
    /* distance is monotone in position along the curve: no sample goes backwards */
    double prev = -1;
    for (int i = 0; i <= 200; ++i) {
        const sat_path2_sample_t s = sample(c, D(sat_path2_length(&c)) * i / 200.0);
        const double d = D(s.distance);
        OK(d >= prev);
        prev = d;
    }
}

int main() {
    line();
    polyline();
    arcs_and_circles();
    beziers();
    arc_length_tables();
    advance_clamp_and_wrap();
    nearest_point();
    repeatability();
    std::puts("PASS: test_path2.cpp");
    return 0;
}
