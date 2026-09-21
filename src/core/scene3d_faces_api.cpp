#include "saturn/scene3d_faces.h"
#include "saturn/vdp1_color_calc.h"
#include "src/core/mesh3d_logic.hpp"
#include "src/core/render3d_logic.hpp"

#include <stdint.h>

namespace {
bool valid_material(const sat_scene3d_material_t& m) {
    if (m.kind==SAT_SCENE3D_RGB)
        return m.color_calc_slot==SAT_INDEXED_SOLID_OPAQUE;
    if (m.kind==SAT_SCENE3D_INDEXED_TILED)
        return m.tiled && m.tiled->full && m.tiled->full->valid &&
            (m.color_calc_slot==SAT_INDEXED_SOLID_OPAQUE ||
             m.color_calc_slot<8u);
    if (m.kind!=SAT_SCENE3D_INDEXED_SOLID &&
        m.kind!=SAT_SCENE3D_INDEXED_TEXTURED) return false;
    return m.texture && m.texture->valid &&
        (m.color_calc_slot==SAT_INDEXED_SOLID_OPAQUE ||
         m.color_calc_slot<8u);
}

/* A face the hardware can draw from its projected corners alone: every
 * corner past the near plane, and none so far off-screen that projection
 * clamped it. Off-screen corners inside that bound are left to the VDP1's
 * system clipping instead of the slow re-projecting fallback in emit(). */
bool safe_projection(const sat_scene3d_faces_t& scene,
                     const sat_projected_vertex_t* vertices,
                     const uint16_t* indices) {
    for (uint8_t c=0;c<4u;++c) {
        const sat_projected_vertex_t& p=vertices[indices[c]];
        if (p.w<scene.near_depth ||
            !saturn::core::render3d::coord_drawable(
                p.x,p.y,scene.width,scene.height)) return false;
    }
    return true;
}

/* One unsigned painter key: pass ascending in the high bits, then camera
 * depth far-to-near in the low bits, which is exactly the order
 * paint_order_buckets emits. Depth keeps 1/4096 of a world unit -- finer
 * than the 1024 buckets resolve -- and stays clear of kPaintSkip. */
constexpr uint32_t kFaceDepthBits=20u;
constexpr uint32_t kFaceDepthMax=(1u<<kFaceDepthBits)-2u;
constexpr uint32_t kFacePassMax=0xFFFu;

uint32_t painter_key(int64_t depth_sum, uint16_t pass) {
    int64_t depth=depth_sum/4/16;
    if (depth<0) depth=0;
    if (depth>static_cast<int64_t>(kFaceDepthMax))
        depth=static_cast<int64_t>(kFaceDepthMax);
    const uint32_t capped=pass>kFacePassMax?kFacePassMax:pass;
    return ((kFacePassMax-capped)<<kFaceDepthBits)|
           static_cast<uint32_t>(depth);
}

sat_result_t append(sat_scene3d_faces_t* scene, const sat_quad3_t& world,
                    const sat_projected_vertex_t* projected,
                    const uint16_t indices[4],
                    const sat_scene3d_material_t& material, uint16_t pass) {
    bool visible=false;
    int64_t sum=0;
    for (uint8_t c=0;c<4u;++c) {
        const sat_fx16_t depth=projected[indices[c]].w;
        sum+=depth;
        if (depth>0) visible=true;
    }
    if (!visible) {
        ++scene->culled_faces;
        return SAT_OK;
    }
    if (scene->count>=scene->capacity) return SAT_ERR_CAPACITY;
    sat_scene3d_face_t& item=scene->entries[scene->count];
    item.world=world;
    for (uint8_t c=0;c<4u;++c) {
        item.projected.x[c]=projected[indices[c]].x;
        item.projected.y[c]=projected[indices[c]].y;
    }
    item.material=material;
    item.gouraud_valid=material.vertex_gouraud ? 1u : 0u;
    if (item.gouraud_valid) {
        for (uint8_t c=0;c<4u;++c)
            item.gouraud[c]=material.vertex_gouraud[indices[c]];
        item.material.vertex_gouraud=nullptr;
    }
    item.projected_safe=safe_projection(*scene,projected,indices)?1u:0u;
    if (!item.projected_safe) ++scene->clipped_faces;
    scene->keys[scene->count]=painter_key(sum,pass);
    ++scene->count;
    return SAT_OK;
}

sat_result_t emit(const sat_scene3d_faces_t& scene,
                  const sat_scene3d_face_t& face) {
    if (face.material.kind==SAT_SCENE3D_RGB) {
        if (!face.projected_safe) return SAT_ERR_UNSUPPORTED;
        if (face.gouraud_valid)
            return sat_draw_quad2_polygon_gouraud(
                &face.projected, face.material.rgb555, face.gouraud);
        return sat_draw_quad2_polygon(&face.projected,face.material.rgb555);
    }
    if (face.projected_safe) {
        sat_distorted_sprite_cmd_t cmd={};
        cmd.texture=face.material.kind==SAT_SCENE3D_INDEXED_TILED
            ? face.material.tiled->full : face.material.texture;
        for (uint8_t i=0u;i<4u;++i) {
            cmd.x[i]=face.projected.x[i];
            cmd.y[i]=face.projected.y[i];
        }
        return face.material.color_calc_slot==SAT_INDEXED_SOLID_OPAQUE
            ? sat_draw_sprite_distorted(&cmd)
            : sat_draw_sprite_distorted_color_calc(
                &cmd,face.material.color_calc_slot);
    }
    sat_indexed_solid_render3d_t p={};
    p.view_proj=&scene.view_proj;
    p.eye=scene.eye;
    p.forward=scene.forward;
    p.near_depth=scene.near_depth;
    p.width=scene.width;p.height=scene.height;
    p.color_calc_slot=face.material.color_calc_slot;
    if (face.material.kind==SAT_SCENE3D_INDEXED_TILED)
        return sat_draw_indexed_tiled_quad3(
            &face.world,&p,face.material.tiled,nullptr);
    if (face.material.kind==SAT_SCENE3D_INDEXED_SOLID)
        return sat_draw_indexed_solid_quad3(
            &face.world,&p,face.material.texture);
    /* Textured UVs cannot be arbitrarily clipped on VDP1. */
    return sat_draw_indexed_textured_quad3(
        &face.world,&p,face.material.texture,nullptr);
}
} // namespace

extern "C" sat_result_t sat_scene3d_faces_init(
    sat_scene3d_faces_t* scene, sat_scene3d_face_t* storage,
    uint32_t* keys, uint16_t* order, uint16_t capacity) {
    if (!scene || !storage || !keys || !order || !capacity)
        return SAT_ERR_INVALID_ARG;
    *scene={};
    scene->entries=storage;scene->keys=keys;scene->order=order;
    scene->capacity=capacity;
    return SAT_OK;
}

extern "C" sat_result_t sat_scene3d_faces_begin(
    sat_scene3d_faces_t* scene, const sat_mat4_t* view_proj,
    const sat_vec3_t* eye, const sat_vec3_t* forward,
    sat_fx16_t near_depth, uint16_t width, uint16_t height) {
    if (!scene || !scene->entries || !scene->capacity || scene->active ||
        !view_proj || !eye || !forward || near_depth<=0 ||
        (!forward->x && !forward->y && !forward->z) ||
        width<2u || height<2u || width>2048u || height>2048u)
        return SAT_ERR_INVALID_ARG;
    scene->count=0;
    scene->culled_faces=scene->clipped_faces=scene->fallback_faces=0u;
    scene->view_proj=*view_proj;
    scene->eye=*eye;scene->forward=*forward;
    scene->near_depth=near_depth;scene->width=width;scene->height=height;
    scene->active=1u;
    return SAT_OK;
}

extern "C" sat_result_t sat_scene3d_faces_begin_camera(
    sat_scene3d_faces_t* scene,const sat_camera3d_t* camera,
    sat_fx16_t near_depth,uint16_t width,uint16_t height) {
    if (!camera || (camera->eye.x==camera->target.x &&
                    camera->eye.y==camera->target.y &&
                    camera->eye.z==camera->target.z))
        return SAT_ERR_INVALID_ARG;
    const sat_vec3_t delta={
        static_cast<sat_fx16_t>(camera->target.x-camera->eye.x),
        static_cast<sat_fx16_t>(camera->target.y-camera->eye.y),
        static_cast<sat_fx16_t>(camera->target.z-camera->eye.z)};
    sat_vec3_t forward={};
    sat_vec3_normalize(&forward,&delta);
    return sat_scene3d_faces_begin(
        scene,&camera->view_proj,&camera->eye,
        &forward,near_depth,width,height);
}

extern "C" sat_result_t sat_scene3d_faces_depth(
    const sat_scene3d_faces_t* scene,const sat_vec3_t* world,
    sat_fx16_t* out_depth) {
    if (!scene || !scene->active || !world || !out_depth)
        return SAT_ERR_INVALID_ARG;
    const int64_t depth=(
        (static_cast<int64_t>(world->x)-scene->eye.x)*scene->forward.x+
        (static_cast<int64_t>(world->y)-scene->eye.y)*scene->forward.y+
        (static_cast<int64_t>(world->z)-scene->eye.z)*scene->forward.z
    ) / SAT_FX16_ONE;
    *out_depth=depth>INT32_MAX?INT32_MAX:
               (depth<INT32_MIN?INT32_MIN:static_cast<sat_fx16_t>(depth));
    return SAT_OK;
}

extern "C" sat_result_t sat_scene3d_faces_submit_quad(
    sat_scene3d_faces_t* scene, const sat_quad3_t* world,
    const sat_scene3d_material_t* material, uint16_t pass) {
    if (!scene || !scene->active || !world || !material ||
        !valid_material(*material)) return SAT_ERR_INVALID_ARG;
    if (scene->count>=scene->capacity) return SAT_ERR_CAPACITY;
    sat_projected_vertex_t screen[4]={};
    const sat_result_t st=sat_project_vertices(
        &scene->view_proj,world->v,4u,screen);
    if (st!=SAT_OK) return st;
    const uint16_t indices[4]={0u,1u,2u,3u};
    return append(scene,*world,screen,indices,*material,pass);
}

extern "C" sat_result_t sat_scene3d_faces_submit_box(
    sat_scene3d_faces_t* scene, const sat_indexed_box3_t* box,
    uint8_t color_calc_slot, uint16_t pass) {
    if (!scene || !scene->active) return SAT_ERR_INVALID_ARG;
    sat_quad3_t faces[3]={};
    const sat_vdp1_texture_t* textures[3]={};
    uint8_t count=0u;
    const sat_result_t st=sat_indexed_box3_faces(
        box,&scene->eye,faces,textures,&count);
    if (st!=SAT_OK) return st;
    if (count>scene->capacity-scene->count) return SAT_ERR_CAPACITY;
    for (uint8_t i=0u;i<count;++i) {
        sat_scene3d_material_t material={};
        material.kind=SAT_SCENE3D_INDEXED_SOLID;
        material.texture=textures[i];
        material.color_calc_slot=color_calc_slot;
        if (!valid_material(material)) return SAT_ERR_INVALID_ARG;
    }
    for (uint8_t i=0u;i<count;++i) {
        sat_scene3d_material_t material={};
        material.kind=SAT_SCENE3D_INDEXED_SOLID;
        material.texture=textures[i];
        material.color_calc_slot=color_calc_slot;
        const sat_result_t submitted=sat_scene3d_faces_submit_quad(
            scene,&faces[i],&material,pass);
        if (submitted!=SAT_OK) return submitted;
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_scene3d_faces_submit_tiled_quad(
    sat_scene3d_faces_t* scene, const sat_quad3_t* world,
    const sat_indexed_tiled_quad3_t* regions,
    uint8_t color_calc_slot, uint16_t pass) {
    sat_scene3d_material_t material={};
    material.kind=SAT_SCENE3D_INDEXED_TILED;
    material.tiled=regions;
    material.color_calc_slot=color_calc_slot;
    return sat_scene3d_faces_submit_quad(scene,world,&material,pass);
}

extern "C" sat_result_t sat_scene3d_faces_submit_instance(
    sat_scene3d_faces_t* scene, const sat_scene3d_instance_t* instance,
    uint8_t color_calc_slot,
    sat_projected_vertex_t* screen_scratch, sat_vec3_t* world_scratch) {
    const sat_mesh_t* mesh=instance ? instance->mesh : nullptr;
    const sat_scene3d_material_t* materials=instance ? instance->materials : nullptr;
    const uint16_t material_count=instance ? instance->material_count : 0u;
    const uint16_t* face_materials=instance ? instance->face_materials : nullptr;
    if (!scene || !scene->active || !instance || !mesh || !mesh->vertices ||
        !mesh->indices || !screen_scratch ||
        (instance->world && !world_scratch) ||
        (mesh->face_count && (!materials || !face_materials)))
        return SAT_ERR_INVALID_ARG;
    if (mesh->face_count>scene->capacity-scene->count)
        return SAT_ERR_CAPACITY;
    /* One submission-wide slot beats one material table per slot: distance
     * fade acts on a whole object, while the material table is shared by
     * every object using the colour. Validate the material the face will
     * ACTUALLY carry, not the one the table holds -- an RGB material is only
     * legal at SAT_INDEXED_SOLID_OPAQUE, so an override has to be rejected
     * here rather than reaching emit(). The override is materialized only
     * when there is one: a 300-face character submitted with
     * SAT_SCENE3D_SLOT_INHERIT must not pay a struct copy per face. */
    const bool override_slot=color_calc_slot!=SAT_SCENE3D_SLOT_INHERIT;
    for (uint16_t f=0;f<mesh->face_count;++f) {
        if (face_materials[f]>=material_count) return SAT_ERR_INVALID_ARG;
        const sat_scene3d_material_t& base=materials[face_materials[f]];
        if (override_slot) {
            sat_scene3d_material_t overridden=base;
            overridden.color_calc_slot=color_calc_slot;
            if (!valid_material(overridden)) return SAT_ERR_INVALID_ARG;
        } else if (!valid_material(base)) {
            return SAT_ERR_INVALID_ARG;
        }
        const uint16_t* idx=&mesh->indices[static_cast<uint32_t>(f)*4u];
        for (uint8_t c=0;c<4u;++c)
            if (idx[c]>=mesh->vertex_count) return SAT_ERR_INVALID_ARG;
    }
    const sat_vec3_t* world=mesh->vertices;
    if (instance->world) {
        for (uint16_t v=0;v<mesh->vertex_count;++v) {
            const sat_vec3_t& p=mesh->vertices[v];
            const sat_vec4_t local={p.x,p.y,p.z,SAT_FX16_ONE};
            sat_vec4_t transformed={};
            const sat_result_t st=sat_mat4_transform_vec4(
                instance->world,&local,&transformed);
            if (st!=SAT_OK) return st;
            world_scratch[v]={transformed.x,transformed.y,transformed.z};
        }
        world=world_scratch;
    }
    const sat_result_t projected=sat_project_vertices(
        &scene->view_proj,world,mesh->vertex_count,screen_scratch);
    if (projected!=SAT_OK) return projected;
    /* After complete preflight all accepted faces are copied into queue
     * storage, never retaining world_scratch or projected scratch pointers. */
    for (uint16_t f=0;f<mesh->face_count;++f) {
        const uint16_t* idx=&mesh->indices[static_cast<uint32_t>(f)*4u];
        if (instance->cull_backfaces &&
            screen_scratch[idx[0]].w>0 && screen_scratch[idx[1]].w>0 &&
            screen_scratch[idx[2]].w>0 && screen_scratch[idx[3]].w>0 &&
            saturn::core::render3d::projected_area2(screen_scratch,idx)<=0)
        {
            ++scene->culled_faces;
            continue;
        }
        sat_quad3_t quad={};
        for (uint8_t c=0;c<4u;++c) quad.v[c]=world[idx[c]];
        /* Keep instance back-face visibility in world space. The projected
         * area test below is still useful for degenerate/quantized screen
         * faces, but it is not a substitute for the shared geometric normal
         * predicate: small perspective faces can round to a screen winding
         * that disagrees with their actual outward normal. */
        if (instance->cull_backfaces &&
            !saturn::core::mesh3d::quad_visible(quad,scene->eye))
        {
            ++scene->culled_faces;
            continue;
        }
        const sat_scene3d_material_t& base=materials[face_materials[f]];
        sat_result_t st;
        if (override_slot) {
            sat_scene3d_material_t overridden=base;
            overridden.color_calc_slot=color_calc_slot;
            st=append(scene,quad,screen_scratch,idx,overridden,instance->pass);
        } else {
            st=append(scene,quad,screen_scratch,idx,base,instance->pass);
        }
        if (st!=SAT_OK) return st;
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_scene3d_faces_flush(
    sat_scene3d_faces_t* scene) {
    if (!scene || !scene->active) return SAT_ERR_INVALID_ARG;
    scene->active=0u;
    /* Orders indices in O(faces + buckets) and never moves a face record;
     * the same helper already carries the native mesh paint order. */
    const uint32_t ordered=saturn::core::render3d::paint_order_buckets(
        scene->keys,scene->count,scene->order);
    sat_result_t result=SAT_OK;
    for (uint32_t i=0;i<ordered;++i) {
        if (!scene->entries[scene->order[i]].projected_safe)
            ++scene->fallback_faces;
        result=emit(*scene,scene->entries[scene->order[i]]);
        if (result==SAT_ERR_UNSUPPORTED) continue;
        if (result!=SAT_OK) break;
    }
    scene->count=0u;
    return result==SAT_ERR_UNSUPPORTED?SAT_OK:result;
}
