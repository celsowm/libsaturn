#ifndef SATURN_CORE_RENDER3D_LOGIC_HPP
#define SATURN_CORE_RENDER3D_LOGIC_HPP

/* Pure, host-testable world-quad projection and shading.
 *
 * No hardware access, so tests/host/test_render3d_logic.cpp links this
 * directly. The public C API in include/saturn/render3d.h is a thin wrapper;
 * only the two drawing entry points there touch the VDP1.
 */

#include <stdint.h>

#include "saturn/core.h"
#include "saturn/render3d.h"
#include "src/core/math3d_logic.hpp"

namespace saturn::core::render3d {

using saturn::core::math3d::fx_mul;
using saturn::core::math3d::fx_to_int;
using saturn::core::math3d::mat4_transform_vec4;

/* VDP1 vertex fields hold far more range than a 320x224 screen needs, but not
 * the full int32 a near-plane projection can produce. Clamping well outside
 * the screen keeps a partly off-screen quad roughly the right shape while
 * making it impossible for a huge value to wrap its sign and fold the quad
 * inside out. */
constexpr int32_t kCoordLimit = 2047;

/* A distorted VDP1 sprite carries no per-vertex UVs, so a textured quad can
 * never be clipped in software without resampling it: it is submitted whole
 * and the VDP1's own system clipping discards whatever falls outside the
 * screen. Off-screen corners are therefore legal, but two bounds still apply.
 * A corner beyond kCoordLimit would be clamped by project_axis, which folds
 * the quad's shape and skews the texture across it; and a sprite stretched
 * far past the screen spends VDP1 cycles on pixels nobody sees. Accept a box
 * this many times the viewport and leave anything larger to the caller's
 * subdivision fallback. */
constexpr int32_t kOffscreenViewportScale = 2;

inline int32_t offscreen_bound(uint16_t extent) {
    const int32_t bound =
        (static_cast<int32_t>(extent) / 2) * kOffscreenViewportScale;
    return bound > kCoordLimit ? kCoordLimit : bound;
}

/* True when a projected corner may go straight to the VDP1 undistorted. */
inline bool coord_drawable(
    int32_t x, int32_t y, uint16_t width, uint16_t height
) {
    const int32_t bx = offscreen_bound(width);
    const int32_t by = offscreen_bound(height);
    return x >= -bx && x <= bx && y >= -by && y <= by;
}

inline int16_t clamp_coord64(int64_t v) {
    if (v < -static_cast<int64_t>(kCoordLimit)) {
        return static_cast<int16_t>(-kCoordLimit);
    }
    if (v > static_cast<int64_t>(kCoordLimit)) {
        return static_cast<int16_t>(kCoordLimit);
    }
    return static_cast<int16_t>(v);
}

/* clamp_coord64(num / w2) for w2 > 0, without a libgcc 64-bit divide.
 *
 * Every projected corner used to pay two __divdi3 calls, which profiling put
 * near a fifth of an animated-model frame. A quotient past the clamp needs no
 * divide at all, and ruling those out first leaves one that fits 32 bits --
 * exactly what the SH-2's hardware divider takes. */
inline int16_t project_axis(int64_t num, int64_t w2) {
    const int64_t limit = w2 * static_cast<int64_t>(kCoordLimit + 1);
    if (num >= limit) {
        return static_cast<int16_t>(kCoordLimit);
    }
    if (num <= -limit) {
        return static_cast<int16_t>(-kCoordLimit);
    }
    if (w2 > static_cast<int64_t>(0x7FFFFFFF)) {
        return static_cast<int16_t>(num / w2);
    }
    return static_cast<int16_t>(
        saturn::core::math3d::div_s64_s32(num, static_cast<int32_t>(w2)));
}

/* Projects one world point straight to NATIVE VDP1 coordinates.
 *
 * The screen-space form is ((ndc + 1) * half) for x and ((1 - ndc) * half)
 * for y; native coordinates put the origin at the screen centre, so the
 * half-screen offset cancels and only the scaled NDC term survives -- which
 * also flips y, since world +Y is up and native +Y is down.
 *
 * The divide and the screen scale are fused into one 64-bit expression rather
 * than going through 16.16 helpers. A point close to the camera plane has a
 * tiny w, and both the normalise and the scale then produce values far outside
 * int32; in 16.16 arithmetic those wrap silently, and a wrapped coordinate
 * flips sign and folds the quad inside out instead of merely clamping to the
 * edge of the screen. Keeping full width until the final clamp makes the
 * clamp the only thing that ever bounds the result. */
inline bool project_native(
    const sat_fx16_t* view_proj,
    sat_fx16_t x,
    sat_fx16_t y,
    sat_fx16_t z,
    int16_t screen_w,
    int16_t screen_h,
    int16_t* out_x,
    int16_t* out_y
) {
    const sat_vec4_t clip = mat4_transform_vec4(view_proj, x, y, z, SAT_FX16_ONE);
    if (clip.w <= 0) {
        return false;
    }
    /* Both operands are 16.16, so the fixed-point scaling cancels in the
     * ratio and the result is already in whole pixels. */
    const int64_t w2 = static_cast<int64_t>(clip.w) * 2;
    *out_x = project_axis(static_cast<int64_t>(clip.x) * static_cast<int64_t>(screen_w), w2);
    *out_y = project_axis(-(static_cast<int64_t>(clip.y) * static_cast<int64_t>(screen_h)), w2);
    return true;
}

/* Projects one point and keeps its clip w; w <= 0 leaves x/y untouched. */
inline void project_vertex(
    const sat_fx16_t* view_proj,
    const sat_vec3_t& p,
    int16_t screen_w,
    int16_t screen_h,
    sat_projected_vertex_t* out
) {
    const sat_vec4_t clip = mat4_transform_vec4(view_proj, p.x, p.y, p.z, SAT_FX16_ONE);
    out->w = clip.w;
    if (clip.w <= 0) {
        return;
    }
    const int64_t w2 = static_cast<int64_t>(clip.w) * 2;
    out->x = project_axis(static_cast<int64_t>(clip.x) * static_cast<int64_t>(screen_w), w2);
    out->y = project_axis(-(static_cast<int64_t>(clip.y) * static_cast<int64_t>(screen_h)), w2);
}

/* Twice the signed area of a quad A, B, C, D from native corner coordinates
 * (y grows downward): the cross product of its diagonals, (C - A) x (D - B),
 * which expands term for term into the shoelace sum -- two products instead
 * of eight. A front face -- A, B, C, D running clockwise on screen, the
 * winding saturn/mesh3d.h specifies -- comes out positive, so this is
 * backface culling without a 3D normal: exact under perspective for any face
 * in front of the camera. A repeated corner needs no special case: with
 * D == C it is the triangle's own cross product. Corners are clamped to
 * +/-2047, so each product fits comfortably in 32 bits. */
inline int32_t corner_area2(
    int32_t ax, int32_t ay, int32_t bx, int32_t by,
    int32_t cx, int32_t cy, int32_t dx, int32_t dy
) {
    return ((cx - ax) * (dy - by)) - ((dx - bx) * (cy - ay));
}

inline int32_t quad2_area2(const sat_quad2_t& q) {
    return corner_area2(q.x[0], q.y[0], q.x[1], q.y[1], q.x[2], q.y[2], q.x[3], q.y[3]);
}

/* The same area straight from projection-cache entries, without building a
 * sat_quad2_t for a face that may be culled anyway. */
inline int32_t projected_area2(const sat_projected_vertex_t* screen, const uint16_t* idx) {
    const sat_projected_vertex_t& a = screen[idx[0]];
    const sat_projected_vertex_t& b = screen[idx[1]];
    const sat_projected_vertex_t& c = screen[idx[2]];
    const sat_projected_vertex_t& d = screen[idx[3]];
    return corner_area2(a.x, a.y, b.x, b.y, c.x, c.y, d.x, d.y);
}

/* A quad is drawable only if every corner is in front of the camera: the VDP1
 * draws from four corners with no clipper, so a straddling quad would be
 * folded across the screen rather than cut at the near plane. */
inline bool project_quad(
    const sat_fx16_t* view_proj,
    const sat_quad3_t* quad,
    int16_t screen_w,
    int16_t screen_h,
    sat_quad2_t* out
) {
    if (view_proj == nullptr || quad == nullptr || out == nullptr) {
        return false;
    }
    for (int i = 0; i < 4; ++i) {
        const sat_vec3_t& v = quad->v[i];
        /* Triangles and fans are quads with a repeated corner: reuse the
         * earlier projection rather than paying for the same point twice. */
        int same = -1;
        for (int j = 0; j < i; ++j) {
            if (quad->v[j].x == v.x && quad->v[j].y == v.y && quad->v[j].z == v.z) {
                same = j;
                break;
            }
        }
        if (same >= 0) {
            out->x[i] = out->x[same];
            out->y[i] = out->y[same];
            continue;
        }
        if (!project_native(view_proj, v.x, v.y, v.z, screen_w, screen_h, &out->x[i], &out->y[i])) {
            return false;
        }
    }
    return true;
}

/* Clip a convex 3D face against the front half-space d>=near_depth.
 * Keeping this before projection avoids folding or enormous distorted
 * sprites when the follow camera approaches a large nearby deck. The
 * un-clipped fast path retains the original face and UV corner order. */
inline int64_t world_view_depth(
    const sat_vec3_t& p,const sat_vec3_t& eye,const sat_vec3_t& forward) {
    return ((static_cast<int64_t>(p.x)-eye.x)*forward.x+
            (static_cast<int64_t>(p.y)-eye.y)*forward.y+
            (static_cast<int64_t>(p.z)-eye.z)*forward.z)/SAT_FX16_ONE;
}
inline sat_vec3_t near_intersection(
    const sat_vec3_t& a,const sat_vec3_t& b,
    int64_t da,int64_t db,int64_t clip_depth) {
    const int64_t numerator=clip_depth-da;
    const int64_t denominator=db-da; /* caller guarantees a strict crossing */
    return sat_vec3_t{
        static_cast<sat_fx16_t>(
            static_cast<int64_t>(a.x)+
            (static_cast<int64_t>(b.x)-a.x)*numerator/denominator),
        static_cast<sat_fx16_t>(
            static_cast<int64_t>(a.y)+
            (static_cast<int64_t>(b.y)-a.y)*numerator/denominator),
        static_cast<sat_fx16_t>(
            static_cast<int64_t>(a.z)+
            (static_cast<int64_t>(b.z)-a.z)*numerator/denominator)
    };
}
inline sat_result_t clip_world_quad_near(
    const sat_quad3_t* quad,const sat_vec3_t* eye,
    const sat_vec3_t* forward,sat_fx16_t near_depth,
    sat_quad3_t out[4],uint8_t* out_count) {
    if(!quad || !eye || !forward || !out || !out_count ||
       near_depth<=0 || (!forward->x && !forward->y && !forward->z))
        return SAT_ERR_INVALID_ARG;
    int64_t depths[4];
    uint8_t in_count=0;
    for(uint8_t i=0;i<4u;++i) {
        depths[i]=world_view_depth(quad->v[i],*eye,*forward);
        if(depths[i]>=near_depth)++in_count;
    }
    if(in_count==4u) {
        out[0]=*quad; *out_count=1u; return SAT_OK;
    }
    if(in_count==0u) {
        *out_count=0u; return SAT_OK;
    }
    sat_vec3_t clipped[6];
    uint8_t n=0u;
    sat_vec3_t previous=quad->v[3];
    int64_t previous_depth=depths[3];
    bool previous_inside=previous_depth>=near_depth;
    for(uint8_t i=0u;i<4u;++i) {
        const sat_vec3_t current=quad->v[i];
        const int64_t current_depth=depths[i];
        const bool current_inside=current_depth>=near_depth;
        if(previous_inside!=current_inside) {
            clipped[n++]=near_intersection(
                previous,current,previous_depth,current_depth,near_depth);
        }
        if(current_inside) clipped[n++]=current;
        previous=current;
        previous_depth=current_depth;
        previous_inside=current_inside;
    }
    if(n<3u) {
        *out_count=0u; return SAT_OK;
    }
    /* A clipped quad has at most 6 vertices, so 4 fan triangles.
     * Degenerate triangle D=C is understood by the existing projector. */
    *out_count=static_cast<uint8_t>(n-2u);
    for(uint8_t i=0u;i<*out_count;++i) {
        out[i].v[0]=clipped[0];
        out[i].v[1]=clipped[i+1u];
        out[i].v[2]=clipped[i+2u];
        out[i].v[3]=clipped[i+2u];
    }
    return SAT_OK;
}

/* Native viewport clipping for UNIFORM 16x16 indexed sprites or polygons.
 * A large projected floor can occupy thousands of pixels outside the screen:
 * relying solely on VDP1's system clip still makes its distorted-sprite
 * rasterizer traverse enormous spans, delaying the HUD and later frames.
 * Convex quad + rectangle intersection yields <=8 unique vertices. */
struct ScreenPoint {int16_t x,y;};
inline bool screen_inside(ScreenPoint p,uint8_t edge,int32_t bound) {
    const int32_t value=(edge<2u)?p.x:p.y;
    return (edge&1u)?value<=bound:value>=bound;
}
inline ScreenPoint screen_intersection(
    ScreenPoint a,ScreenPoint b,uint8_t edge,int32_t bound) {
    const int32_t av=edge<2u?a.x:a.y;
    const int32_t bv=edge<2u?b.x:b.y;
    const int32_t delta=bv-av;
    ScreenPoint out=a;
    if(edge<2u) {
        out.x=static_cast<int16_t>(bound);
        out.y=static_cast<int16_t>(
            static_cast<int64_t>(a.y)+
            (static_cast<int64_t>(b.y)-a.y)*(bound-av)/delta);
    } else {
        out.y=static_cast<int16_t>(bound);
        out.x=static_cast<int16_t>(
            static_cast<int64_t>(a.x)+
            (static_cast<int64_t>(b.x)-a.x)*(bound-av)/delta);
    }
    return out;
}
inline bool screen_equal(ScreenPoint a,ScreenPoint b) {
    return a.x==b.x && a.y==b.y;
}
inline sat_result_t clip_quad_screen(
    const sat_quad2_t* quad,uint16_t width,uint16_t height,
    sat_quad2_t out[6],uint8_t* out_count) {
    if(!quad || !out || !out_count || width<2u || height<2u ||
       width>2048u || height>2048u)
        return SAT_ERR_INVALID_ARG;
    const int32_t bounds[4]={
        -static_cast<int32_t>(width)/2,
        static_cast<int32_t>(width+1u)/2-1,
        -static_cast<int32_t>(height)/2,
        static_cast<int32_t>(height+1u)/2-1
    };
    bool fully_inside=true;
    ScreenPoint previous[12],next[12];
    uint8_t n=0u;
    for(uint8_t i=0u;i<4u;++i) {
        const ScreenPoint point={quad->x[i],quad->y[i]};
        if(n==0u || !screen_equal(point,previous[n-1u]))
            previous[n++]=point;
        for(uint8_t edge=0u;edge<4u;++edge)
            if(!screen_inside(point,edge,bounds[edge]))fully_inside=false;
    }
    if(fully_inside) {
        out[0]=*quad;
        *out_count=1u;
        return SAT_OK;
    }
    if(n>1u && screen_equal(previous[n-1u],previous[0]))--n;
    for(uint8_t edge=0u;edge<4u && n>=3u;++edge) {
        uint8_t produced=0u;
        ScreenPoint last=previous[n-1u];
        bool was_inside=screen_inside(last,edge,bounds[edge]);
        for(uint8_t i=0u;i<n;++i) {
            const ScreenPoint current=previous[i];
            const bool inside=screen_inside(current,edge,bounds[edge]);
            if(inside!=was_inside) {
                if(produced>=12u)return SAT_ERR_UNSUPPORTED;
                next[produced++]=screen_intersection(
                    last,current,edge,bounds[edge]);
            }
            if(inside) {
                if(produced>=12u)return SAT_ERR_UNSUPPORTED;
                next[produced++]=current;
            }
            last=current;
            was_inside=inside;
        }
        n=0u;
        for(uint8_t i=0u;i<produced;++i) {
            if(n==0u || !screen_equal(previous[n-1u],next[i]))
                previous[n++]=next[i];
        }
        if(n>1u && screen_equal(previous[n-1u],previous[0]))--n;
    }
    if(n<3u) {*out_count=0u;return SAT_OK;}
    if(n>8u)return SAT_ERR_UNSUPPORTED;
    uint8_t triangles=0u;
    for(uint8_t i=1u;i+1u<n;++i) {
        const ScreenPoint p0=previous[0],p1=previous[i],
                          p2=previous[i+1u];
        const int64_t area=
            static_cast<int64_t>(p1.x-p0.x)*(p2.y-p0.y)-
            static_cast<int64_t>(p1.y-p0.y)*(p2.x-p0.x);
        if(!area)continue;
        sat_quad2_t& result=out[triangles++];
        result.x[0]=p0.x;result.y[0]=p0.y;
        result.x[1]=p1.x;result.y[1]=p1.y;
        result.x[2]=p2.x;result.y[2]=p2.y;
        result.x[3]=p2.x;result.y[3]=p2.y;
    }
    *out_count=triangles;
    return SAT_OK;
}

/* ------------------------------------------------------------------ */
/* Quad construction                                                   */
/* ------------------------------------------------------------------ */

inline void make_wall(
    sat_quad3_t* out,
    sat_fx16_t x0,
    sat_fx16_t z0,
    sat_fx16_t x1,
    sat_fx16_t z1,
    sat_fx16_t height
) {
    if (out == nullptr) {
        return;
    }
    out->v[0].x = x0; out->v[0].y = height; out->v[0].z = z0;
    out->v[1].x = x1; out->v[1].y = height; out->v[1].z = z1;
    out->v[2].x = x1; out->v[2].y = 0;      out->v[2].z = z1;
    out->v[3].x = x0; out->v[3].y = 0;      out->v[3].z = z0;
}

inline void make_floor(
    sat_quad3_t* out,
    sat_fx16_t cx,
    sat_fx16_t y,
    sat_fx16_t cz,
    sat_fx16_t half
) {
    if (out == nullptr) {
        return;
    }
    const sat_fx16_t x0 = cx - half;
    const sat_fx16_t x1 = cx + half;
    const sat_fx16_t z0 = cz - half;
    const sat_fx16_t z1 = cz + half;
    out->v[0].x = x0; out->v[0].y = y; out->v[0].z = z0;
    out->v[1].x = x1; out->v[1].y = y; out->v[1].z = z0;
    out->v[2].x = x1; out->v[2].y = y; out->v[2].z = z1;
    out->v[3].x = x0; out->v[3].y = y; out->v[3].z = z1;
}

inline void make_billboard(
    sat_quad3_t* out,
    sat_fx16_t cx,
    sat_fx16_t cz,
    sat_fx16_t right_x,
    sat_fx16_t right_z,
    sat_fx16_t half_w,
    sat_fx16_t height
) {
    if (out == nullptr) {
        return;
    }
    const sat_fx16_t ox = fx_mul(right_x, half_w);
    const sat_fx16_t oz = fx_mul(right_z, half_w);
    out->v[0].x = cx - ox; out->v[0].y = height; out->v[0].z = cz - oz;
    out->v[1].x = cx + ox; out->v[1].y = height; out->v[1].z = cz + oz;
    out->v[2].x = cx + ox; out->v[2].y = 0;      out->v[2].z = cz + oz;
    out->v[3].x = cx - ox; out->v[3].y = 0;      out->v[3].z = cz - oz;
}

/* ------------------------------------------------------------------ */
/* Painter's algorithm                                                 */
/* ------------------------------------------------------------------ */

/* Painter's order: larger key first, equal keys by ascending index -- which,
 * for an ascending input list, is exactly what a stable sort produces. The
 * index tie-break is what lets an unstable heapsort give that answer. */
template <typename Index>
inline bool paints_before(Index a, Index b, const uint32_t* keys) {
    return keys[a] > keys[b] || (keys[a] == keys[b] && a < b);
}

template <typename Index>
inline void sift_down(Index* indices, const uint32_t* keys, uint32_t root, uint32_t end) {
    const Index value = indices[root];
    for (;;) {
        uint32_t child = (root * 2u) + 1u;
        if (child >= end) {
            break;
        }
        if (child + 1u < end && paints_before(indices[child], indices[child + 1u], keys)) {
            ++child;
        }
        if (!paints_before(value, indices[child], keys)) {
            break;
        }
        indices[root] = indices[child];
        root = child;
    }
    indices[root] = value;
}

/* Heapsort: O(n log n) worst case, no scratch. It replaced an insertion sort
 * whose "near-linear on coherent orders" never applied here -- sat_vdp1_draw_mesh
 * rebuilds the list in index order every frame, so the quadratic case was
 * the ordinary one. */
template <typename Index>
inline void sort_painter(Index* indices, const uint32_t* keys, uint32_t count) {
    if (count < 2u) {
        return;
    }
    for (uint32_t i = count / 2u; i > 0u; --i) {
        sift_down(indices, keys, i - 1u, count);
    }
    for (uint32_t end = count - 1u; end > 0u; --end) {
        const Index last = indices[0];
        indices[0] = indices[end];
        indices[end] = last;
        sift_down(indices, keys, 0u, end);
    }
}

inline void sort_indices_desc(uint8_t* indices, const uint32_t* keys, uint16_t count) {
    if (indices == nullptr || keys == nullptr || count > 255u) {
        return;
    }
    sort_painter(indices, keys, count);
}

/* Wide form of the painter's sort for meshes past the 255-face uint8_t
 * limit (animated characters near the VDP1 command budget); `count` may
 * reach 65535. */
inline void sort_indices16_desc(uint16_t* indices, const uint32_t* keys, uint32_t count) {
    if (indices == nullptr || keys == nullptr || count > 65535u) {
        return;
    }
    sort_painter(indices, keys, count);
}

/* Marks a face paint_order_buckets leaves out (culled, or not drawable). */
constexpr uint32_t kPaintSkip = 0xFFFFFFFFu;
constexpr uint32_t kPaintBuckets = 1024u;

/* Farthest-first draw order in O(faces + buckets), for sat_vdp1_draw_mesh.
 *
 * depth[f] holds face f's key (larger = farther) or kPaintSkip. The live key
 * range is spread over kPaintBuckets by a power-of-two shift -- no divide --
 * and faces come out bucket by bucket, farthest first, in ascending face
 * index inside a bucket. Faces closer in depth than one bucket (about a
 * thousandth of the mesh's depth span) can therefore keep index order where
 * the heapsort this replaced ordered them exactly; that heapsort spent
 * O(n log n) indirect key comparisons doing it, which an unbiased profile put
 * at most of an animated character's frame. depth[] is overwritten with
 * bucket numbers. Returns the number of faces written to out[]. */
template <typename Index>
inline uint32_t paint_order_buckets(uint32_t* depth, uint32_t face_count, Index* out) {
    uint32_t lo = 0xFFFFFFFFu;
    uint32_t hi = 0u;
    uint32_t live = 0u;
    for (uint32_t f = 0; f < face_count; ++f) {
        const uint32_t key = depth[f];
        if (key == kPaintSkip) {
            continue;
        }
        if (key < lo) {
            lo = key;
        }
        if (key > hi) {
            hi = key;
        }
        ++live;
    }
    if (live == 0u) {
        return 0u;
    }
    uint32_t shift = 0u;
    while (((hi - lo) >> shift) >= kPaintBuckets) {
        ++shift;
    }
    /* face_count fits uint16_t, so every running position does too. */
    uint16_t start[kPaintBuckets];
    for (uint32_t b = 0; b < kPaintBuckets; ++b) {
        start[b] = 0u;
    }
    for (uint32_t f = 0; f < face_count; ++f) {
        if (depth[f] == kPaintSkip) {
            continue;
        }
        const uint32_t bucket = (depth[f] - lo) >> shift;
        depth[f] = bucket;
        ++start[bucket];
    }
    uint32_t running = 0u;
    for (uint32_t b = kPaintBuckets; b-- > 0u;) {
        const uint32_t n = start[b];
        start[b] = static_cast<uint16_t>(running);
        running += n;
    }
    for (uint32_t f = 0; f < face_count; ++f) {
        const uint32_t bucket = depth[f];
        if (bucket == kPaintSkip) {
            continue;
        }
        out[start[bucket]++] = static_cast<Index>(f);
    }
    return live;
}

/* Distances between maze-scale points fit comfortably in 32 bits, but a
 * degenerate camera position must not wrap the key and invert the sort. */
inline uint32_t ground_distance_sq(sat_fx16_t ax, sat_fx16_t az, sat_fx16_t bx, sat_fx16_t bz) {
    const int64_t dx = static_cast<int64_t>(fx_to_int(ax)) - static_cast<int64_t>(fx_to_int(bx));
    const int64_t dz = static_cast<int64_t>(fx_to_int(az)) - static_cast<int64_t>(fx_to_int(bz));
    const int64_t d = (dx * dx) + (dz * dz);
    if (d > static_cast<int64_t>(0xFFFFFFFFu)) {
        return 0xFFFFFFFFu;
    }
    return static_cast<uint32_t>(d);
}

/* ------------------------------------------------------------------ */
/* Flat shading                                                        */
/* ------------------------------------------------------------------ */

inline uint16_t shade_rgb555(uint16_t rgb555, sat_fx16_t intensity) {
    if (intensity < 0) {
        intensity = 0;
    }
    const uint16_t code = static_cast<uint16_t>(rgb555 & 0x8000u);
    uint32_t channel[3];
    channel[0] = static_cast<uint32_t>((rgb555 >> 10u) & 0x1Fu);
    channel[1] = static_cast<uint32_t>((rgb555 >> 5u) & 0x1Fu);
    channel[2] = static_cast<uint32_t>(rgb555 & 0x1Fu);
    for (int i = 0; i < 3; ++i) {
        uint32_t v = static_cast<uint32_t>(
            (static_cast<int64_t>(channel[i]) * static_cast<int64_t>(intensity)) >> 16);
        if (v > 31u) {
            v = 31u;
        }
        channel[i] = v;
    }
    return static_cast<uint16_t>(
        code | (channel[0] << 10u) | (channel[1] << 5u) | channel[2]);
}

/* Fixed directional light, normalised on the ground plane. Chosen off-axis so
 * that the four axis-aligned wall orientations of a maze all receive distinct
 * intensities -- with a light parallel to an axis, two of them would match and
 * corners would disappear. */
constexpr sat_fx16_t kLightX = 42427;  /* ~0.6474 */
constexpr sat_fx16_t kLightZ = 49933;  /* ~0.7619 */

/* The same light lifted off the ground plane, for solids whose faces point in
 * any direction. Its horizontal component runs the same way as kLightX/kLightZ
 * so a scene mixing walls with meshes stays consistent; the +Y component is
 * what stops every top face from being the same brightness as the floor. */
constexpr sat_fx16_t kLight3X = 25458;  /* ~0.3884 */
constexpr sat_fx16_t kLight3Y = 52429;  /* ~0.8000 */
constexpr sat_fx16_t kLight3Z = 29959;  /* ~0.4571 */

inline sat_fx16_t clamp_floor(sat_fx16_t floor_intensity) {
    if (floor_intensity < 0) {
        return 0;
    }
    if (floor_intensity > SAT_FX16_ONE) {
        return SAT_FX16_ONE;
    }
    return floor_intensity;
}

/* Intensity for a normal of ANY length.
 *
 * dot(n_hat, L) is dot(n, L) / |n|, so an unnormalised normal costs one
 * square root and one 64-bit divide here instead of the root and three
 * divides normalising it would have cost first. That matters: with a mesh
 * scene the normal path was measured at 64% of the frame. */
inline sat_fx16_t face_intensity3_scaled(const sat_vec3_t& normal, sat_fx16_t floor_intensity) {
    const sat_fx16_t floor_value = clamp_floor(floor_intensity);
    const sat_vec3_t light = {kLight3X, kLight3Y, kLight3Z};
    const int64_t raw = saturn::core::math3d::vec3_dot_raw(normal, light);
    if (raw <= 0) {
        return floor_value;
    }
    const sat_fx16_t len =
        saturn::core::math3d::fx_len3(normal.x, normal.y, normal.z);
    if (len == 0) {
        return floor_value;
    }
    sat_fx16_t dot = static_cast<sat_fx16_t>(raw / static_cast<int64_t>(len));
    if (dot > SAT_FX16_ONE) {
        dot = SAT_FX16_ONE;
    }
    return floor_value + fx_mul(SAT_FX16_ONE - floor_value, dot);
}

inline sat_fx16_t face_intensity3(const sat_vec3_t& normal, sat_fx16_t floor_intensity) {
    const sat_fx16_t floor_value = clamp_floor(floor_intensity);
    sat_fx16_t dot = static_cast<sat_fx16_t>(
        fx_mul(normal.x, kLight3X) + fx_mul(normal.y, kLight3Y) + fx_mul(normal.z, kLight3Z));
    if (dot < 0) {
        dot = 0;
    }
    if (dot > SAT_FX16_ONE) {
        dot = SAT_FX16_ONE;
    }
    return floor_value + fx_mul(SAT_FX16_ONE - floor_value, dot);
}

inline sat_fx16_t face_intensity(sat_fx16_t nx, sat_fx16_t nz, sat_fx16_t floor_intensity) {
    if (floor_intensity < 0) {
        floor_intensity = 0;
    }
    if (floor_intensity > SAT_FX16_ONE) {
        floor_intensity = SAT_FX16_ONE;
    }
    sat_fx16_t dot = fx_mul(nx, kLightX) + fx_mul(nz, kLightZ);
    if (dot < 0) {
        dot = 0;
    }
    if (dot > SAT_FX16_ONE) {
        dot = SAT_FX16_ONE;
    }
    /* floor + (1 - floor) * dot */
    return floor_intensity + fx_mul(SAT_FX16_ONE - floor_intensity, dot);
}

/* ------------------------------------------------------------------ */
/* Gouraud shading                                                     */
/* ------------------------------------------------------------------ */

/* One white-Gouraud table entry: the same correction, clamped to -16..+15
 * levels, on all three channels (10h is "no change"). */
inline uint16_t gouraud_grey_word(int32_t delta) {
    if (delta < -16) {
        delta = -16;
    }
    if (delta > 15) {
        delta = 15;
    }
    const uint16_t v = static_cast<uint16_t>(delta + 16);
    return static_cast<uint16_t>((v << 10u) | (v << 5u) | v);
}

/* (intensity - 1) * 16 levels, rounded: 1.0 keeps the part color. */
inline uint16_t gouraud_from_intensity(sat_fx16_t intensity) {
    const int64_t scaled = (static_cast<int64_t>(intensity) - SAT_FX16_ONE) * 16;
    return gouraud_grey_word(static_cast<int32_t>((scaled + 32768) >> 16));
}

inline uint16_t gouraud_lambert_word(
    const sat_vec3_t& normal,
    const sat_vec3_t& light,
    sat_fx16_t ambient
) {
    const sat_fx16_t floor_value = clamp_floor(ambient);
    sat_fx16_t d = saturn::core::math3d::vec3_dot(normal, light);
    if (d < 0) {
        d = 0;
    }
    if (d > SAT_FX16_ONE) {
        d = SAT_FX16_ONE;
    }
    return gouraud_from_intensity(floor_value + fx_mul(SAT_FX16_ONE - floor_value, d));
}

/* gouraud_lambert_word plus a specular push: past `highlight_start` the
 * corner is driven brighter than the part colour instead of just less dim,
 * which gouraud_from_intensity can express because its delta goes both
 * directions. `highlight_gain` is a plain integer scale on the fx16
 * (d - highlight_start) term, not an fx16 multiplicand -- one 16.16 value
 * times a small integer count is the ordinary way to scale a fixed-point
 * quantity by a unitless factor, the same as a loop stepping by `n * unit`. */
inline uint16_t gouraud_lambert_highlight_word(
    const sat_vec3_t& normal,
    const sat_vec3_t& light,
    sat_fx16_t ambient,
    sat_fx16_t highlight_start,
    int32_t highlight_gain
) {
    const sat_fx16_t floor_value = clamp_floor(ambient);
    sat_fx16_t d = saturn::core::math3d::vec3_dot(normal, light);
    sat_fx16_t intensity;
    if (d < 0) {
        d = 0;
    }
    if (d > SAT_FX16_ONE) {
        d = SAT_FX16_ONE;
    }
    intensity = floor_value + fx_mul(SAT_FX16_ONE - floor_value, d);
    if (d > highlight_start) {
        intensity += (d - highlight_start) * highlight_gain;
    }
    return gouraud_from_intensity(intensity);
}

}  // namespace saturn::core::render3d

#endif /* SATURN_CORE_RENDER3D_LOGIC_HPP */
