#include <cmath>
#include <cstdio>

#include "saturn/vdp2_floor.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); return 1; } } while (0)

namespace {

constexpr double kOne = 65536.0;
constexpr uint16_t kWidth = 320, kHeight = 224;

sat_fx16_t fx(double v) { return static_cast<sat_fx16_t>(std::lround(v * kOne)); }

bool camera(double ex, double ey, double ez, double tx, double ty, double tz, double ux,
            sat_mat4_t* view_proj, sat_vec3_t* eye) {
    const sat_vec3_t target = {fx(tx), fx(ty), fx(tz)}, up = {fx(ux), fx(1.0), 0};
    sat_mat4_t view, projection;
    *eye = {fx(ex), fx(ey), fx(ez)};
    return sat_mat4_look_at(&view, eye, &target, &up) == SAT_OK &&
           sat_mat4_perspective(&projection, fx(60.0), fx(320.0 / 224.0), fx(0.25), fx(40.0)) == SAT_OK &&
           sat_mat4_multiply(view_proj, &projection, &view) == SAT_OK;
}

/* Solves the 3x3 system a x = b (Cramer). */
bool solve(const double a[3][3], const double b[3], double x[3]) {
    const double d = a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1]) -
                     a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0]) +
                     a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]);
    if (std::fabs(d) < 1e-12) return false;
    for (int k = 0; k < 3; ++k) {
        double c[3][3];
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) c[i][j] = j == k ? b[i] : a[i][j];
        x[k] = (c[0][0] * (c[1][1] * c[2][2] - c[1][2] * c[2][1]) -
                c[0][1] * (c[1][0] * c[2][2] - c[1][2] * c[2][0]) +
                c[0][2] * (c[1][0] * c[2][1] - c[1][1] * c[2][0])) / d;
    }
    return true;
}

/* The floor point (y = 0) the matrix shows at screen dot (h, v): clip x and
 * y are the NDC times clip w, a linear system in x, z and w. */
bool floor_at(const sat_mat4_t& m, double h, double v, double* x, double* z) {
    const double nx = (h - kWidth / 2) / (kWidth / 2), ny = (kHeight / 2 - v) / (kHeight / 2);
    double r[16];
    for (int i = 0; i < 16; ++i) r[i] = m.m[i] / kOne;
    /* r0.p + r3 = nx w, r4.p + r7 = ny w, r12.p + r15 = w with p = (x, 0, z). */
    const double a[3][3] = {{r[0], r[2], -nx}, {r[4], r[6], -ny}, {r[12], r[14], -1.0}};
    const double b[3] = {-r[3], -r[7], -r[15]};
    double s[3];
    if (!solve(a, b, s) || s[2] <= 0.25) return false;
    *x = s[0];
    *z = s[1];
    return true;
}

double coefficient(uint32_t word) {
    const int32_t v = static_cast<int32_t>(word << 8) >> 8;
    return v / kOne;
}

/* Largest map error in dots over the visible screen. */
double worst(const sat_vdp2_floor_t& floor, const sat_mat4_t& m, const sat_vdp2_rotation_params_t& p,
             const uint32_t* k, unsigned* checked) {
    double e = 0;
    for (int v = 0; v < kHeight; v += 7) {
        if (k[v] & 0x80000000u) continue;
        for (int h = 0; h < kWidth; h += 9) {
            double x, z;
            if (!floor_at(m, h, v, &x, &z)) continue;
            const double xs = (p.xst + h * p.dx) / kOne, ys = (p.yst + v * p.dyst) / kOne;
            const double c = coefficient(k[v]);
            const double mx = c * (p.matrix[0] * xs + p.matrix[1] * ys) / kOne + p.mx / kOne;
            const double my = c * (p.matrix[3] * xs + p.matrix[4] * ys) / kOne + p.my / kOne;
            const double ex = (floor.origin_x + floor.density * x) / kOne;
            const double ez = (floor.origin_y + floor.density * z) / kOne;
            e = std::fmax(e, std::fmax(std::fabs(mx - ex), std::fabs(my - ez)));
            ++*checked;
        }
    }
    return e;
}

}  // namespace

int main() {
    const sat_vdp2_floor_t floor = {kWidth, kHeight, 0, 8 << 16, 100 << 16, 50 << 16, 0};
    struct { double e[3], t[3]; } cases[] = {
        {{0, 3, -9}, {0, 1, 0}},          /* looking along +z */
        {{0, 2, 9}, {0, 1, 0}},           /* along -z */
        {{4, 2.5, 6}, {-1, 0.8, -2}},     /* yawed */
        {{-3, 6, -2}, {1, 0, 3}},         /* steep */
        {{2, 1.2, 1}, {9, 1.5, 4}},       /* level, slightly up */
    };
    sat_mat4_t m;
    sat_vec3_t eye;
    sat_vdp2_rotation_params_t p;
    uint32_t k[kHeight];
    for (const auto& c : cases) {
        unsigned checked = 0;
        OK(camera(c.e[0], c.e[1], c.e[2], c.t[0], c.t[1], c.t[2], 0, &m, &eye));
        OK(sat_vdp2_floor_from_view(&floor, &m, &eye, fx(0.25), fx(40.0), &p, k) == SAT_OK);
        const double e = worst(floor, m, p, k, &checked);
        std::printf("eye (%g %g %g): %u dots, worst %.3f map dots\n", c.e[0], c.e[1], c.e[2], checked, e);
        OK(checked > 200u && e < 0.75);
    }
    /* The top line of a level camera is above the horizon. */
    OK(camera(0, 2, -9, 0, 2, 0, 0, &m, &eye));
    OK(sat_vdp2_floor_from_view(&floor, &m, &eye, fx(0.25), fx(40.0), &p, k) == SAT_OK);
    OK((k[0] & 0x80000000u) != 0u && (k[kHeight - 1] & 0x80000000u) == 0u);
    /* The repeat keeps the map offsets in one period. */
    sat_vdp2_floor_t repeat = floor;
    repeat.period = 64 << 16;
    OK(camera(300, 3, -9, 300, 1, 0, 0, &m, &eye));
    OK(sat_vdp2_floor_from_view(&repeat, &m, &eye, fx(0.25), fx(40.0), &p, k) == SAT_OK);
    OK(p.mx >= 0 && p.mx < repeat.period && p.my >= 0 && p.my < repeat.period);
    /* Per-line far depths cut lines farther than their entry; 0 keeps far_z. */
    {
        static sat_fx16_t far_lines[kHeight / 2];
        sat_vdp2_floor_t cut = floor;
        uint32_t plain[kHeight];
        OK(camera(0, 3, -9, 0, 1, 0, 0, &m, &eye));
        OK(sat_vdp2_floor_from_view(&floor, &m, &eye, fx(0.25), fx(40.0), &p, plain) == SAT_OK);
        for (unsigned i = 0; i < kHeight / 2; ++i) far_lines[i] = i < kHeight / 4 ? fx(6.0) : 0;
        cut.far_lines = far_lines;
        cut.far_step = 2;
        OK(sat_vdp2_floor_from_view(&cut, &m, &eye, fx(0.25), fx(40.0), &p, k) == SAT_OK);
        unsigned removed = 0;
        for (unsigned y = 0; y < kHeight; ++y) {
            if (y >= kHeight / 2) OK(k[y] == plain[y]);
            else if (k[y] != plain[y]) { OK((k[y] & 0x80000000u) != 0u); ++removed; }
        }
        OK(removed > 0u);
        cut.far_step = 0;
        OK(sat_vdp2_floor_from_view(&cut, &m, &eye, fx(0.25), fx(40.0), &p, k) == SAT_ERR_INVALID_ARG);
    }
    /* A rolled camera and one below the plane are unsupported. */
    OK(camera(0, 3, -9, 0, 1, 0, 0.3, &m, &eye));
    OK(sat_vdp2_floor_from_view(&floor, &m, &eye, fx(0.25), fx(40.0), &p, k) == SAT_ERR_UNSUPPORTED);
    OK(camera(0, -1, -9, 0, 1, 0, 0, &m, &eye));
    OK(sat_vdp2_floor_from_view(&floor, &m, &eye, fx(0.25), fx(40.0), &p, k) == SAT_ERR_UNSUPPORTED);
    OK(sat_vdp2_floor_from_view(nullptr, &m, &eye, fx(0.25), fx(40.0), &p, k) == SAT_ERR_INVALID_ARG);
    std::puts("vdp2 floor ok");
    return 0;
}
