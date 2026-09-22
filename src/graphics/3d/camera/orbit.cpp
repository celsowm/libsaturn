#include "saturn/orbit_camera3d.h"
#include <limits.h>

namespace {

bool checked_mul(sat_fx16_t x, sat_fx16_t factor, sat_fx16_t* out) {
    const int64_t value=(static_cast<int64_t>(x)*factor)>>16;
    if(value<INT32_MIN || value>INT32_MAX) return false;
    *out=static_cast<sat_fx16_t>(value);
    return true;
}

sat_fx16_t greater(sat_fx16_t a,sat_fx16_t b) {
    return a>b ? a : b;
}

} // namespace

extern "C" sat_result_t sat_orbit_camera3d_update(sat_orbit_camera3d_t* c) {
    if(c==nullptr || c->distance<=0 || c->near_z<=0 ||
       c->far_z<=c->near_z || c->aspect<=0 || c->fov_y<=0 ||
       c->fov_y>=sat_fx16_from_int(180) ||
       c->min_distance<=0 || c->max_distance<c->min_distance ||
       c->distance<c->min_distance || c->distance>c->max_distance ||
       c->pitch_min_deg>c->pitch_max_deg ||
       c->pitch_deg<c->pitch_min_deg || c->pitch_deg>c->pitch_max_deg)
        return SAT_ERR_INVALID_ARG;

    const sat_fx16_t yaw=sat_fx16_from_int(c->yaw_deg);
    const sat_fx16_t pitch=sat_fx16_from_int(c->pitch_deg);
    const sat_fx16_t r=sat_fx16_mul(c->distance,sat_cos_deg(pitch));
    const int64_t x=static_cast<int64_t>(c->target.x)+sat_fx16_mul(r,sat_sin_deg(yaw));
    const int64_t y=static_cast<int64_t>(c->target.y)+sat_fx16_mul(c->distance,sat_sin_deg(pitch));
    const int64_t z=static_cast<int64_t>(c->target.z)+sat_fx16_mul(r,sat_cos_deg(yaw));
    if(x<INT32_MIN || x>INT32_MAX || y<INT32_MIN || y>INT32_MAX ||
       z<INT32_MIN || z>INT32_MAX)
        return SAT_ERR_INVALID_ARG;

    const sat_vec3_t eye={
        static_cast<sat_fx16_t>(x),
        static_cast<sat_fx16_t>(y),
        static_cast<sat_fx16_t>(z)
    };
    const sat_vec3_t up={0,SAT_FX16_ONE,0};
    sat_mat4_t view={},projection={},vp={};
    SAT_TRY(sat_mat4_look_at(&view,&eye,&c->target,&up));
    SAT_TRY(sat_mat4_perspective(
        &projection,c->fov_y,c->aspect,c->near_z,c->far_z));
    SAT_TRY(sat_mat4_multiply(&vp,&projection,&view));
    c->eye=eye;
    c->view_proj=vp;
    return SAT_OK;
}

extern "C" sat_result_t sat_orbit_camera3d_reset(sat_orbit_camera3d_t* c) {
    if(c==nullptr) return SAT_ERR_INVALID_ARG;
    sat_orbit_camera3d_t next=*c;
    next.yaw_deg=0;
    next.pitch_deg=10;
    if(next.pitch_deg<next.pitch_min_deg) next.pitch_deg=next.pitch_min_deg;
    if(next.pitch_deg>next.pitch_max_deg) next.pitch_deg=next.pitch_max_deg;
    next.distance=next.default_distance;
    next.auto_orbit=0u;
    SAT_TRY(sat_orbit_camera3d_update(&next));
    *c=next;
    return SAT_OK;
}

extern "C" sat_result_t sat_orbit_camera3d_fit_bounds(
    sat_orbit_camera3d_t* out,const sat_vec3_t* mn,
    const sat_vec3_t* mx,const sat_orbit_camera3d_fit_t* p
) {
    if(out==nullptr || mn==nullptr || mx==nullptr || p==nullptr ||
       p->min_extent<=0 || p->min_distance_floor<0 ||
       p->initial_distance_factor<=0 || p->min_distance_factor<=0 ||
       p->max_distance_factor<=0 || p->near_plane_factor<0 ||
       p->near_plane_floor<=0 || p->aspect<=0 ||
       p->fov_y<=0 || p->fov_y>=sat_fx16_from_int(180) ||
       p->far_z<=0 || p->pitch_min_deg>p->pitch_max_deg ||
       p->pitch_min_deg<=-90 || p->pitch_max_deg>=90 ||
       mn->x>mx->x || mn->y>mx->y || mn->z>mx->z)
        return SAT_ERR_INVALID_ARG;

    const int64_t dx=static_cast<int64_t>(mx->x)-mn->x;
    const int64_t dy=static_cast<int64_t>(mx->y)-mn->y;
    const int64_t dz=static_cast<int64_t>(mx->z)-mn->z;
    const int64_t ext=(dx>=dy && dx>=dz)?dx:((dy>=dz)?dy:dz);
    if(ext>INT32_MAX) return SAT_ERR_INVALID_ARG;
    const sat_fx16_t size=greater(static_cast<sat_fx16_t>(ext),p->min_extent);
    sat_orbit_camera3d_t next={};
    next.target={
        static_cast<sat_fx16_t>(static_cast<int64_t>(mn->x)+dx/2),
        static_cast<sat_fx16_t>(static_cast<int64_t>(mn->y)+dy/2),
        static_cast<sat_fx16_t>(static_cast<int64_t>(mn->z)+dz/2)
    };
    sat_fx16_t init=0,min_dist=0,max_dist=0,near=0;
    if(!checked_mul(size,p->initial_distance_factor,&init) ||
       !checked_mul(size,p->min_distance_factor,&min_dist) ||
       !checked_mul(size,p->max_distance_factor,&max_dist) ||
       !checked_mul(size,p->near_plane_factor,&near))
        return SAT_ERR_INVALID_ARG;
    next.min_distance=greater(min_dist,p->min_distance_floor);
    next.max_distance=greater(max_dist,next.min_distance);
    next.default_distance=greater(init,next.min_distance);
    if(next.default_distance>next.max_distance)
        next.default_distance=next.max_distance;
    next.near_z=greater(near,p->near_plane_floor);
    next.far_z=p->far_z;
    next.fov_y=p->fov_y;
    next.aspect=p->aspect;
    next.pitch_min_deg=p->pitch_min_deg;
    next.pitch_max_deg=p->pitch_max_deg;
    if(next.min_distance<=0 || next.far_z<=next.near_z ||
       next.default_distance<=0) return SAT_ERR_INVALID_ARG;
    SAT_TRY(sat_orbit_camera3d_reset(&next));
    *out=next;
    return SAT_OK;
}

extern "C" sat_result_t sat_orbit_camera3d_apply_pad(
    sat_orbit_camera3d_t* c,const sat_pad_state_t* pad,
    uint16_t auto_toggle_button
) {
    if(c==nullptr || pad==nullptr ||
       (auto_toggle_button!=0u &&
        (auto_toggle_button&(auto_toggle_button-1u))!=0u))
        return SAT_ERR_INVALID_ARG;
    sat_orbit_camera3d_t next=*c;
    if((pad->held&SAT_PAD_LEFT)!=0u) next.yaw_deg-=2;
    if((pad->held&SAT_PAD_RIGHT)!=0u) next.yaw_deg+=2;
    if((pad->held&SAT_PAD_UP)!=0u) next.pitch_deg+=2;
    if((pad->held&SAT_PAD_DOWN)!=0u) next.pitch_deg-=2;
    if(next.pitch_deg<next.pitch_min_deg) next.pitch_deg=next.pitch_min_deg;
    if(next.pitch_deg>next.pitch_max_deg) next.pitch_deg=next.pitch_max_deg;
    if((pad->held&SAT_PAD_L)!=0u) {
        const int64_t step=next.distance/40+1;
        const int64_t zoom=static_cast<int64_t>(next.distance)+step;
        next.distance=static_cast<sat_fx16_t>(
            zoom>next.max_distance?next.max_distance:zoom);
    }
    if((pad->held&SAT_PAD_R)!=0u) {
        const int64_t step=next.distance/40+1;
        const int64_t zoom=static_cast<int64_t>(next.distance)-step;
        next.distance=static_cast<sat_fx16_t>(
            zoom<next.min_distance?next.min_distance:zoom);
    }
    if(auto_toggle_button!=0u && (pad->pressed&auto_toggle_button)!=0u)
        next.auto_orbit=static_cast<uint8_t>(!next.auto_orbit);
    if(next.auto_orbit!=0u) ++next.yaw_deg;
    next.yaw_deg%=360;
    if(next.yaw_deg<0) next.yaw_deg+=360;
    if((pad->pressed&SAT_PAD_B)!=0u) {
        SAT_TRY(sat_orbit_camera3d_reset(&next));
    } else {
        SAT_TRY(sat_orbit_camera3d_update(&next));
    }
    *c=next;
    return SAT_OK;
}
