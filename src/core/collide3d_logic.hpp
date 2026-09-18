#ifndef SATURN_CORE_COLLIDE3D_LOGIC_HPP
#define SATURN_CORE_COLLIDE3D_LOGIC_HPP

#include <stdint.h>
#include <limits.h>
#include "saturn/collide3d.h"
#include "src/core/math3d_logic.hpp"
#include "src/core/mesh3d_logic.hpp"

namespace saturn::core::collide3d {
using namespace saturn::core::math3d;
using saturn::core::mesh3d::quad_normal_scaled;

inline sat_vec3_t add(sat_vec3_t a, sat_vec3_t b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
inline sat_vec3_t sub(sat_vec3_t a, sat_vec3_t b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
inline sat_vec3_t mul(sat_vec3_t a, sat_fx16_t s) {
    return {static_cast<sat_fx16_t>((static_cast<int64_t>(a.x)*s)>>16),
            static_cast<sat_fx16_t>((static_cast<int64_t>(a.y)*s)>>16),
            static_cast<sat_fx16_t>((static_cast<int64_t>(a.z)*s)>>16)};
}
inline sat_fx16_t ab(sat_fx16_t v) { return v < 0 ? -v : v; }
inline uint64_t len2(sat_vec3_t v) {
    return static_cast<uint64_t>(static_cast<int64_t>(v.x)*v.x) +
           static_cast<uint64_t>(static_cast<int64_t>(v.y)*v.y) +
           static_cast<uint64_t>(static_cast<int64_t>(v.z)*v.z);
}
inline sat_vec3_t unit(sat_vec3_t v) {
    const sat_fx16_t l = fx_len3(v.x,v.y,v.z);
    return l ? sat_vec3_t{fx_div(v.x,l),fx_div(v.y,l),fx_div(v.z,l)} : sat_vec3_t{0,0,0};
}
inline sat_fx16_t ratio(int64_t n, int64_t d) {
    if (!d) return 0;
    while (d > INT32_MAX || d < INT32_MIN) { n >>= 1; d >>= 1; if (!d) return 0; }
    if (n > (INT64_MAX>>16)) n = INT64_MAX>>16;
    if (n < (INT64_MIN>>16)) n = INT64_MIN>>16;
    return static_cast<sat_fx16_t>(div_s64_s32(n<<16,static_cast<int32_t>(d)));
}
inline sat_vec3_t at(sat_vec3_t o, sat_vec3_t d, sat_fx16_t t) { return add(o,mul(d,t)); }

inline bool aabb_overlap(const sat_aabb3_t& a,const sat_aabb3_t& b) {
    const int64_t dx=static_cast<int64_t>(a.center.x)-b.center.x;
    const int64_t dy=static_cast<int64_t>(a.center.y)-b.center.y;
    const int64_t dz=static_cast<int64_t>(a.center.z)-b.center.z;
    return dx < static_cast<int64_t>(a.half.x)+b.half.x && dx > -static_cast<int64_t>(a.half.x)-b.half.x &&
           dy < static_cast<int64_t>(a.half.y)+b.half.y && dy > -static_cast<int64_t>(a.half.y)-b.half.y &&
           dz < static_cast<int64_t>(a.half.z)+b.half.z && dz > -static_cast<int64_t>(a.half.z)-b.half.z;
}
inline bool sphere_overlap(const sat_sphere_t& a,const sat_sphere_t& b) {
    const int64_t r=static_cast<int64_t>(a.radius)+b.radius;
    return len2(sub(a.center,b.center)) < static_cast<uint64_t>(r*r);
}
inline sat_vec3_t clamp(sat_vec3_t p,const sat_aabb3_t& b) {
    sat_vec3_t q=p;
    const sat_fx16_t lo[3]={b.center.x-b.half.x,b.center.y-b.half.y,b.center.z-b.half.z};
    const sat_fx16_t hi[3]={b.center.x+b.half.x,b.center.y+b.half.y,b.center.z+b.half.z};
    sat_fx16_t* v[3]={&q.x,&q.y,&q.z};
    for(int i=0;i<3;++i) { if(*v[i]<lo[i])*v[i]=lo[i]; if(*v[i]>hi[i])*v[i]=hi[i]; }
    return q;
}
inline bool sphere_aabb(const sat_sphere_t& s,const sat_aabb3_t& b) {
    const sat_vec3_t d=sub(s.center,clamp(s.center,b));
    if (len2(d)!=0) return len2(d)<static_cast<uint64_t>(static_cast<int64_t>(s.radius)*s.radius);
    return true;
}
inline int sphere_contact(const sat_sphere_t& a,const sat_sphere_t& b,sat_contact3_t& o) {
    const sat_vec3_t d=sub(a.center,b.center); const uint64_t q=len2(d);
    const int64_t r=static_cast<int64_t>(a.radius)+b.radius;
    if(q>=static_cast<uint64_t>(r*r))return 0;
    const sat_fx16_t l=static_cast<sat_fx16_t>(isqrt64(q));
    o.normal=l?unit(d):sat_vec3_t{SAT_FX16_ONE,0,0}; o.depth=static_cast<sat_fx16_t>(r-l); return 1;
}
inline int aabb_contact(const sat_aabb3_t& a,const sat_aabb3_t& b,sat_contact3_t& o) {
    const int64_t dx=static_cast<int64_t>(a.center.x)-b.center.x,dy=static_cast<int64_t>(a.center.y)-b.center.y,dz=static_cast<int64_t>(a.center.z)-b.center.z;
    const int64_t px=static_cast<int64_t>(a.half.x)+b.half.x-ab(static_cast<sat_fx16_t>(dx));
    const int64_t py=static_cast<int64_t>(a.half.y)+b.half.y-ab(static_cast<sat_fx16_t>(dy));
    const int64_t pz=static_cast<int64_t>(a.half.z)+b.half.z-ab(static_cast<sat_fx16_t>(dz));
    if(px<=0||py<=0||pz<=0)return 0;
    if(px<=py&&px<=pz){o.normal={dx<0?-SAT_FX16_ONE:SAT_FX16_ONE,0,0};o.depth=px;}
    else if(py<=pz){o.normal={0,dy<0?-SAT_FX16_ONE:SAT_FX16_ONE,0};o.depth=py;}
    else{o.normal={0,0,dz<0?-SAT_FX16_ONE:SAT_FX16_ONE};o.depth=pz;} return 1;
}
inline int sphere_box_contact(const sat_sphere_t& s,const sat_aabb3_t& b,sat_contact3_t& o) {
    const sat_vec3_t q=clamp(s.center,b),d=sub(s.center,q); const uint64_t q2=len2(d);
    if(q2){if(q2>=static_cast<uint64_t>(static_cast<int64_t>(s.radius)*s.radius))return 0;const sat_fx16_t l=static_cast<sat_fx16_t>(isqrt64(q2));o.normal=unit(d);o.depth=s.radius-l;return 1;}
    const sat_fx16_t ds[3]={static_cast<sat_fx16_t>(b.half.x-ab(s.center.x-b.center.x)),static_cast<sat_fx16_t>(b.half.y-ab(s.center.y-b.center.y)),static_cast<sat_fx16_t>(b.half.z-ab(s.center.z-b.center.z))};
    int k=0;if(ds[1]<ds[k])k=1;if(ds[2]<ds[k])k=2;o.normal={0,0,0};sat_fx16_t* n[3]={&o.normal.x,&o.normal.y,&o.normal.z};*n[k]=(s.center.x==b.center.x&&k==0)||(s.center.y==b.center.y&&k==1)||(s.center.z==b.center.z&&k==2)?SAT_FX16_ONE:((k==0?s.center.x-b.center.x:k==1?s.center.y-b.center.y:s.center.z-b.center.z)<0?-SAT_FX16_ONE:SAT_FX16_ONE);o.depth=s.radius+ds[k];return o.depth>0;
}
inline int plane_contact(const sat_sphere_t& s,const sat_plane3_t& p,sat_contact3_t& o) {
    const sat_vec3_t n=unit(p.normal);if(n.x==0&&n.y==0&&n.z==0)return 0;
    const sat_fx16_t d=static_cast<sat_fx16_t>(vec3_dot_raw(sub(s.center,p.point),n)>>16);
    if (ab(d) >= s.radius) return 0;
    o.normal = d < 0 ? mul(n, -SAT_FX16_ONE) : n;
    o.depth = s.radius - ab(d);
    return 1;
}
inline bool ray_box(const sat_aabb3_t& b,const sat_ray3_t& r,sat_hit3_t& o) {
    const sat_vec3_t d=mul(r.dir,r.length); const sat_fx16_t lo[3]={b.center.x-b.half.x,b.center.y-b.half.y,b.center.z-b.half.z};const sat_fx16_t hi[3]={b.center.x+b.half.x,b.center.y+b.half.y,b.center.z+b.half.z};const sat_fx16_t ov[3]={r.origin.x,r.origin.y,r.origin.z},dv[3]={d.x,d.y,d.z};sat_fx16_t en=0,ex=SAT_FX16_ONE; sat_vec3_t n={0,0,0};
    for(int k=0;k<3;++k){if(!dv[k]){if(ov[k]<=lo[k]||ov[k]>=hi[k])return false;continue;}sat_fx16_t a=ratio(static_cast<int64_t>(lo[k])-ov[k],dv[k]),bb=ratio(static_cast<int64_t>(hi[k])-ov[k],dv[k]);sat_vec3_t nn={0,0,0};if(a>bb){sat_fx16_t t=a;a=bb;bb=t;nn.x=(k==0)?SAT_FX16_ONE:0;nn.y=(k==1)?SAT_FX16_ONE:0;nn.z=(k==2)?SAT_FX16_ONE:0;}else{nn.x=(k==0)?-SAT_FX16_ONE:0;nn.y=(k==1)?-SAT_FX16_ONE:0;nn.z=(k==2)?-SAT_FX16_ONE:0;}if(a>en){en=a;n=nn;}if(bb<ex)ex=bb;if(en>ex||ex<0||en>SAT_FX16_ONE)return false;}
    if (en < 0) en = 0;
    o.t = en; o.point = at(r.origin, d, en); o.normal = n; o.face = 0xFFFF;
    return true;
}
inline bool ray_sphere(const sat_sphere_t& s,const sat_ray3_t& r,sat_hit3_t& o){
    const sat_vec3_t d=mul(r.dir,r.length),f=sub(r.origin,s.center);const uint64_t aa=len2(d);if(!aa)return false;const int64_t bb=2*vec3_dot_raw(f,d),cc=static_cast<int64_t>(len2(f))-static_cast<int64_t>(s.radius)*s.radius,disc=bb*bb-4*static_cast<int64_t>(aa)*cc;if(disc<0)return false;const int64_t den=2*static_cast<int64_t>(aa),root=isqrt64(static_cast<uint64_t>(disc));sat_fx16_t t=ratio(-bb-root,den);if(t<0)t=0;if(t>SAT_FX16_ONE){t=ratio(-bb+root,den);if(t<0||t>SAT_FX16_ONE)return false;}o.t=t;o.point=at(r.origin,d,t);o.normal=unit(sub(o.point,s.center));o.face=0xFFFF;return true;
}
inline bool inside_quad(const sat_quad3_t& q,sat_vec3_t p,sat_vec3_t n){for(int i=0;i<4;++i){const sat_vec3_t a=q.v[i],b=q.v[(i+1)&3];if(vec3_dot_raw(vec3_cross_scaled(sub(b,a),sub(p,a)),n)>0)return false;}return true;}
inline bool ray_quad(const sat_quad3_t& q,const sat_ray3_t& r,sat_hit3_t& o){const sat_vec3_t n=quad_normal_scaled(q);if(!n.x&&!n.y&&!n.z)return false;const sat_vec3_t d=mul(r.dir,r.length);const int64_t den=vec3_dot_raw(n,d);if(!den)return false;const int64_t num=vec3_dot_raw(n,sub(q.v[0],r.origin));const sat_fx16_t t=ratio(num,den);if(t<0||t>SAT_FX16_ONE)return false;const sat_vec3_t p=at(r.origin,d,t);if(!inside_quad(q,p,n))return false;o.t=t;o.point=p;o.normal=unit(n);o.face=0xFFFF;return true;}

inline bool mesh_bounds(const sat_mesh_t& m, sat_aabb3_t& b) {
    if (!m.vertices || !m.vertex_count) return false;
    sat_vec3_t lo = m.vertices[0], hi = lo;
    for (uint16_t i = 1; i < m.vertex_count; ++i) {
        const sat_vec3_t v = m.vertices[i];
        if (v.x < lo.x) lo.x = v.x; if (v.y < lo.y) lo.y = v.y; if (v.z < lo.z) lo.z = v.z;
        if (v.x > hi.x) hi.x = v.x; if (v.y > hi.y) hi.y = v.y; if (v.z > hi.z) hi.z = v.z;
    }
    b.center = {(lo.x + hi.x) / 2, (lo.y + hi.y) / 2, (lo.z + hi.z) / 2};
    b.half = {(hi.x - lo.x) / 2, (hi.y - lo.y) / 2, (hi.z - lo.z) / 2};
    return true;
}

inline bool ray_mesh(const sat_mesh_t& m, const sat_ray3_t& r, sat_hit3_t& o) {
    sat_aabb3_t b;
    if (!mesh_bounds(m, b)) return false;
    sat_hit3_t broad;
    if (!ray_box(b, r, broad)) return false;
    int found = 0;
    for (uint16_t i = 0; i < m.face_count; ++i) {
        sat_quad3_t q;
        if (saturn::core::mesh3d::face_quad(&m, i, &q) != SAT_OK) continue;
        sat_hit3_t h;
        if (ray_quad(q, r, h) && (!found || h.t < o.t)) {
            h.face = i; o = h; found = 1;
        }
    }
    return found;
}

inline bool sphere_quad_contact(const sat_quad3_t& q, const sat_sphere_t& s, sat_contact3_t& c) {
    const sat_vec3_t n = unit(quad_normal_scaled(q));
    if (!n.x && !n.y && !n.z) return false;
    const sat_fx16_t signed_d =
        static_cast<sat_fx16_t>(vec3_dot_raw(sub(s.center, q.v[0]), n) >> 16);
    const sat_vec3_t proj = sub(s.center, mul(n, signed_d));
    sat_vec3_t closest = proj;
    if (!inside_quad(q, proj, n)) {
        sat_fx16_t best = s.radius + 1;
        for (int e = 0; e < 4; ++e) {
            const sat_vec3_t a = q.v[e];
            const sat_vec3_t d = sub(q.v[(e + 1) & 3], a);
            const uint64_t dd = len2(d);
            sat_fx16_t t = dd
                ? ratio(vec3_dot_raw(sub(s.center, a), d), static_cast<int64_t>(dd))
                : 0;
            if (t < 0) t = 0;
            if (t > SAT_FX16_ONE) t = SAT_FX16_ONE;
            const sat_vec3_t p = at(a, d, t);
            const sat_fx16_t dist = static_cast<sat_fx16_t>(isqrt64(len2(sub(s.center, p))));
            if (dist < best) { best = dist; closest = p; }
        }
    }
    const sat_vec3_t diff = sub(s.center, closest);
    const sat_fx16_t dist = static_cast<sat_fx16_t>(isqrt64(len2(diff)));
    if (dist >= s.radius) return false;
    c.normal = dist ? unit(diff) : (signed_d < 0 ? mul(n, -SAT_FX16_ONE) : n);
    c.depth = s.radius - dist;
    return true;
}

inline sat_result_t sphere_mesh(
    const sat_mesh_t& m,
    const sat_sphere_t& s,
    sat_contact3_t* out,
    uint16_t cap,
    uint16_t& count
) {
    count = 0;
    sat_result_t result = SAT_OK;
    for (uint16_t i = 0; i < m.face_count; ++i) {
        sat_quad3_t q;
        if (saturn::core::mesh3d::face_quad(&m, i, &q) != SAT_OK) continue;
        sat_contact3_t c;
        if (!sphere_quad_contact(q, s, c)) continue;
        if (count >= cap) { result = SAT_ERR_CAPACITY; continue; }
        out[count++] = c;
    }
    return result;
}
}
#endif
