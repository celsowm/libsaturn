#include "saturn/collide3d.h"
#include <limits.h>
#include "src/core/collide3d_logic.hpp"

namespace {
using namespace saturn::core::collide3d;

/* Only a face-interior crossing is reported. A radius-thick edge/corner cast
 * is intentionally not approximated by an infinite plane, as that would
 * recreate invisible floors outside finite platforms. */
sat_result_t cast_faces(const sat_mesh_t& mesh, const sat_sphere_t& sphere,
                        const sat_vec3_t& displacement,
                        sat_sphere_mesh_face_hit_t& out, uint8_t& found) {
    found = 0;
    if (!mesh.vertices || !mesh.indices || !mesh.face_count ||
        !mesh.vertex_count || mesh.face_count > mesh.face_cap ||
        mesh.vertex_count > mesh.vertex_cap || sphere.radius <= 0)
        return SAT_ERR_INVALID_ARG;
    /* face_quad trusts indices: reject malformed meshes before dereferencing
     * vertices and before touching the caller's hit/output parameters. */
    for(uint32_t i=0;i<static_cast<uint32_t>(mesh.face_count)*4u;++i)
        if(mesh.indices[i]>=mesh.vertex_count) return SAT_ERR_INVALID_ARG;
    const int64_t nx = static_cast<int64_t>(sphere.center.x)+displacement.x;
    const int64_t ny = static_cast<int64_t>(sphere.center.y)+displacement.y;
    const int64_t nz = static_cast<int64_t>(sphere.center.z)+displacement.z;
    if (nx<INT32_MIN || nx>INT32_MAX || ny<INT32_MIN || ny>INT32_MAX ||
        nz<INT32_MIN || nz>INT32_MAX) return SAT_ERR_INVALID_ARG;
    const sat_vec3_t end{
        static_cast<sat_fx16_t>(nx),static_cast<sat_fx16_t>(ny),
        static_cast<sat_fx16_t>(nz)};
    for (uint16_t face=0; face<mesh.face_count; ++face) {
        sat_quad3_t q{};
        if (saturn::core::mesh3d::face_quad(&mesh,face,&q)!=SAT_OK)
            return SAT_ERR_INVALID_ARG;
        const sat_vec3_t n=unit(quad_normal_scaled(q));
        if (!n.x && !n.y && !n.z) continue;
        const sat_vec3_t start_offset=sub(sphere.center,q.v[0]);
        const sat_vec3_t end_offset=sub(end,q.v[0]);
        const int64_t start_dot=vec3_dot_raw(start_offset,n)>>16;
        const int64_t end_dot=vec3_dot_raw(end_offset,n)>>16;
        int side=0;
        if (start_dot>=sphere.radius && end_dot<start_dot &&
            end_dot<=sphere.radius) side=1;
        else if (start_dot<=-static_cast<int64_t>(sphere.radius) &&
                 end_dot>start_dot &&
                 end_dot>=-static_cast<int64_t>(sphere.radius)) side=-1;
        if (!side) continue;
        const int64_t numerator=start_dot-
            static_cast<int64_t>(side)*sphere.radius;
        const int64_t denominator=start_dot-end_dot;
        if (!denominator) continue;
        const sat_fx16_t t=ratio(numerator,denominator);
        if (t<0 || t>SAT_FX16_ONE || (found && t>=out.t)) continue;
        const sat_vec3_t center=at(sphere.center,displacement,t);
        const sat_fx16_t signed_distance=static_cast<sat_fx16_t>(
            vec3_dot_raw(sub(center,q.v[0]),n)>>16);
        const sat_vec3_t surface=sub(center,mul(n,signed_distance));
        if (!inside_quad(q,surface,n)) continue;
        out.t=t;
        out.center=center;
        out.point=surface;
        out.normal=side>0?n:mul(n,-SAT_FX16_ONE);
        out.face=face;
        found=1;
    }
    return SAT_OK;
}
} // namespace

extern "C" sat_result_t sat_sphere_cast_mesh_faces(
    const sat_mesh_t* mesh, const sat_sphere_t* sphere,
    const sat_vec3_t* displacement, sat_sphere_mesh_face_hit_t* out,
    uint8_t* found) {
    if(!mesh||!sphere||!displacement||!out||!found)
        return SAT_ERR_INVALID_ARG;
    sat_sphere_mesh_face_hit_t candidate{};
    uint8_t is_hit=0;
    const sat_result_t status=cast_faces(
        *mesh,*sphere,*displacement,candidate,is_hit);
    if(status!=SAT_OK)return status;
    *found=is_hit;
    if(is_hit)*out=candidate;
    return SAT_OK;
}
