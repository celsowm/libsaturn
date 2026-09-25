#include "saturn/scene3d_faces.h"
#include "saturn/parallel.h"
#include "saturn/vdp1_color_calc.h"
#include "src/core/geometry/mesh_logic.hpp"
#include "src/graphics/3d/rendering/logic.hpp"
#include "src/hal/sh2/frt.hpp"

#include <stdint.h>

#ifndef SAT_PROFILE_METRICS
#define SAT_PROFILE_METRICS 0
#endif

namespace {
#if SAT_PROFILE_METRICS
uint32_t g_test_painter_ticks;
uint32_t g_test_emit_ticks;
#endif

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
 * paint_order_grouped_buckets emits. Depth keeps 1/4096 of a world unit --
 * finer than the buckets resolve -- and stays clear of kPaintSkip; faces
 * beyond 256 world units saturate and keep submission order. */
constexpr uint32_t kFaceDepthBits=20u;
constexpr uint32_t kFaceDepthMax=(1u<<kFaceDepthBits)-2u;
constexpr uint32_t kFacePassMax=SAT_SCENE3D_PASS_MAX;

uint32_t painter_key(int64_t depth_sum, uint16_t pass) {
    int64_t depth=depth_sum/4/16;
    if (depth<0) depth=0;
    if (depth>static_cast<int64_t>(kFaceDepthMax))
        depth=static_cast<int64_t>(kFaceDepthMax);
    return ((kFacePassMax-static_cast<uint32_t>(pass))<<kFaceDepthBits)|
           static_cast<uint32_t>(depth);
}

/* `corners` point at the face's four world-space vertices. They are copied
 * into the record only when the face needs the clipping fallback: a
 * projected-safe face never reads `world`, and every byte a record carries
 * is written (uncached, on the Slave) and copied again on merge. */
sat_result_t append(sat_scene3d_faces_t* scene,
                    const sat_vec3_t* const corners[4],
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
    for (uint8_t c=0;c<4u;++c) {
        item.projected.x[c]=projected[indices[c]].x;
        item.projected.y[c]=projected[indices[c]].y;
    }
    item.material=material;
    item.cached_projected=0u;
    item.gouraud_valid=material.vertex_gouraud ? 1u : 0u;
    if (item.gouraud_valid) {
        for (uint8_t c=0;c<4u;++c)
            item.gouraud[c]=material.vertex_gouraud[indices[c]];
        item.material.vertex_gouraud=nullptr;
    }
    item.projected_safe=safe_projection(*scene,projected,indices)?1u:0u;
    if (!item.projected_safe) {
        for (uint8_t c=0;c<4u;++c) item.world.v[c]=*corners[c];
        ++scene->clipped_faces;
    }
    scene->keys[scene->count]=painter_key(sum,pass);
    ++scene->count;
    return SAT_OK;
}

/* Copies the fields a queued face actually carries: `gouraud` only when
 * valid and `world` only for the clipping fallback, about half of the
 * record for an ordinary projected-safe face. */
void copy_face(sat_scene3d_face_t& dst, const sat_scene3d_face_t& src) {
    dst.projected=src.projected;
    dst.material=src.material;
    dst.projected_safe=src.projected_safe;
    dst.gouraud_valid=src.gouraud_valid;
    dst.cached_projected=src.cached_projected;
    if (src.gouraud_valid)
        for (uint8_t c=0;c<4u;++c) dst.gouraud[c]=src.gouraud[c];
    if (!src.projected_safe) dst.world=src.world;
}

sat_result_t emit(const sat_scene3d_faces_t& scene,
                  const sat_scene3d_face_t& face) {
    /* Projected cache items use these SAME RGB/indexed emission paths as
     * world faces; projected_safe forbids any world-space fallback. */
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

extern "C" sat_result_t sat_scene3d_faces_bind_owner_validator(
    sat_scene3d_faces_t* scene,
    sat_scene3d_owner_validate_fn validate,void* context,
    sat_scene3d_owner_span_t* spans,uint16_t span_capacity) {
    if(!scene || scene->active)return SAT_ERR_INVALID_ARG;
    if(validate && (!spans || !span_capacity))return SAT_ERR_INVALID_ARG;
    scene->validate_owner=validate;
    scene->owner_context=validate?context:nullptr;
    scene->owner_spans=validate?spans:nullptr;
    scene->owner_span_capacity=validate?span_capacity:0u;
    scene->owner_span_count=0u;
    return SAT_OK;
}

extern "C" uint8_t sat_scene3d_faces_owner_span_available(
    const sat_scene3d_faces_t* scene) {
    return scene && scene->validate_owner &&
           scene->owner_span_count<scene->owner_span_capacity ? 1u : 0u;
}

extern "C" sat_result_t sat_scene3d_faces_mark_owner(
    sat_scene3d_faces_t* scene,uint16_t first,uint16_t count,
    uint16_t slot,uint16_t generation,const sat_vdp1_texture_t* native) {
    if(!scene || !scene->active || !scene->validate_owner || !native ||
       !count || static_cast<uint32_t>(first)+count>scene->count)
        return SAT_ERR_INVALID_ARG;
    if(scene->owner_span_count>=scene->owner_span_capacity)
        return SAT_ERR_CAPACITY;
    scene->owner_spans[scene->owner_span_count++]=
        sat_scene3d_owner_span_t{first,count,slot,generation,native};
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
    scene->owner_span_count=0u;
    scene->culled_faces=scene->clipped_faces=scene->fallback_faces=0u;
    scene->emitted_faces=scene->emitted_cached=scene->skipped_faces=0u;
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
        pass>SAT_SCENE3D_PASS_MAX || !valid_material(*material))
        return SAT_ERR_INVALID_ARG;
    if (scene->count>=scene->capacity) return SAT_ERR_CAPACITY;
    sat_projected_vertex_t screen[4]={};
    const sat_result_t st=sat_project_vertices(
        &scene->view_proj,world->v,4u,screen);
    if (st!=SAT_OK) return st;
    const uint16_t indices[4]={0u,1u,2u,3u};
    const sat_vec3_t* const corners[4]={
        &world->v[0],&world->v[1],&world->v[2],&world->v[3]};
    return append(scene,corners,screen,indices,*material,pass);
}

extern "C" sat_result_t sat_scene3d_faces_submit_projected_material(
    sat_scene3d_faces_t* scene, const sat_quad2_t* projected,
    sat_fx16_t camera_depth, const sat_scene3d_material_t* material,
    uint16_t pass) {
    if (!scene || !scene->active || !projected || !material ||
        camera_depth < 0 || pass > SAT_SCENE3D_PASS_MAX ||
        material->vertex_gouraud != nullptr || !valid_material(*material))
        return SAT_ERR_INVALID_ARG;
    if (scene->count >= scene->capacity) return SAT_ERR_CAPACITY;
    sat_scene3d_face_t& item=scene->entries[scene->count];
    item.projected=*projected;
    item.material=*material;
    /* A cached projected tile never needs fallback/UV clipping. Bake its
     * full preuploaded texture directly into the copied material descriptor,
     * so the caller's tiled object can expire immediately after submission.
     * Only the actual texture and its VRAM backing must live until flush. */
    if (material->kind==SAT_SCENE3D_INDEXED_TILED) {
        item.material.kind=SAT_SCENE3D_INDEXED_TEXTURED;
        item.material.texture=material->tiled->full;
        item.material.tiled=nullptr;
    }
    item.projected_safe=1u;
    item.cached_projected=1u;
    item.gouraud_valid=0u;
    scene->keys[scene->count]=painter_key(
        static_cast<int64_t>(camera_depth)*4,pass);
    ++scene->count;
    return SAT_OK;
}

extern "C" sat_result_t sat_scene3d_faces_submit_projected_rgb(
    sat_scene3d_faces_t* scene, const sat_quad2_t* projected,
    sat_fx16_t camera_depth, uint16_t color, uint16_t pass) {
    const sat_scene3d_material_t material={
        SAT_SCENE3D_RGB,color,nullptr,nullptr,SAT_INDEXED_SOLID_OPAQUE,nullptr};
    return sat_scene3d_faces_submit_projected_material(
        scene,projected,camera_depth,&material,pass);
}

extern "C" sat_result_t sat_scene3d_faces_submit_box(
    sat_scene3d_faces_t* scene, const sat_indexed_box3_t* box,
    uint8_t color_calc_slot, uint16_t pass) {
    if (!scene || !scene->active || pass>SAT_SCENE3D_PASS_MAX)
        return SAT_ERR_INVALID_ARG;
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
    /* A compound box is ONE queue operation. Individual faces may project
     * successfully before a later projection returns an error; revert only
     * this box's queue/culling statistics, preserving older scene entries.
     * No VDP1 commands have been emitted at this stage. */
    const uint16_t previous_count=scene->count;
    const uint16_t previous_culled=scene->culled_faces;
    const uint16_t previous_clipped=scene->clipped_faces;
    for (uint8_t i=0u;i<count;++i) {
        sat_scene3d_material_t material={};
        material.kind=SAT_SCENE3D_INDEXED_SOLID;
        material.texture=textures[i];
        material.color_calc_slot=color_calc_slot;
        const sat_result_t submitted=sat_scene3d_faces_submit_quad(
            scene,&faces[i],&material,pass);
        if (submitted!=SAT_OK) {
            scene->count=previous_count;
            scene->culled_faces=previous_culled;
            scene->clipped_faces=previous_clipped;
            return submitted;
        }
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
    if (!scene || !scene->active || !instance ||
        instance->pass>SAT_SCENE3D_PASS_MAX ||
        !mesh || !mesh->vertices || !mesh->indices ||
        !mesh->vertex_count || !mesh->face_count ||
        mesh->vertex_count>mesh->vertex_cap ||
        mesh->face_count>mesh->face_cap ||
        !screen_scratch ||
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
    /* Preserve queue atomicity if a future append path fails after other
     * instance faces have already been appended or culled. */
    const uint16_t previous_count=scene->count;
    const uint16_t previous_culled=scene->culled_faces;
    const uint16_t previous_clipped=scene->clipped_faces;
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
        const sat_vec3_t* const corners[4]={
            &world[idx[0]],&world[idx[1]],&world[idx[2]],&world[idx[3]]};
        /* Keep instance back-face visibility in world space. The projected
         * area test below is still useful for degenerate/quantized screen
         * faces, but it is not a substitute for the shared geometric normal
         * predicate: small perspective faces can round to a screen winding
         * that disagrees with their actual outward normal. */
        if (instance->cull_backfaces &&
            !saturn::core::mesh3d::quad_visible(
                *corners[0],*corners[1],*corners[2],*corners[3],scene->eye))
        {
            ++scene->culled_faces;
            continue;
        }
        const sat_scene3d_material_t& base=materials[face_materials[f]];
        sat_result_t st;
        if (override_slot) {
            sat_scene3d_material_t overridden=base;
            overridden.color_calc_slot=color_calc_slot;
            st=append(scene,corners,screen_scratch,idx,overridden,instance->pass);
        } else {
            st=append(scene,corners,screen_scratch,idx,base,instance->pass);
        }
        if (st!=SAT_OK) {
            scene->count=previous_count;
            scene->culled_faces=previous_culled;
            scene->clipped_faces=previous_clipped;
            return st;
        }
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_scene3d_prepare_batch_init(
    sat_scene3d_prepare_batch_t* batch,
    const sat_scene3d_prepare_item_t* items, uint16_t item_count,
    sat_scene3d_face_t* faces, uint32_t* keys, uint16_t capacity) {
    if (batch == nullptr || (item_count != 0u && items == nullptr) ||
        faces == nullptr || keys == nullptr || capacity == 0u)
        return SAT_ERR_INVALID_ARG;
    *batch = {};
    batch->items = items;
    batch->item_count = item_count;
    batch->capacity = capacity;
    batch->faces = faces;
    batch->keys = keys;
    return SAT_OK;
}

extern "C" sat_result_t sat_scene3d_prepare_batch_execute(
    sat_scene3d_prepare_batch_t* batch) {
    if (batch == nullptr || batch->faces == nullptr || batch->keys == nullptr ||
        batch->capacity == 0u ||
        (batch->item_count != 0u && batch->items == nullptr) ||
        batch->near_depth <= 0 || batch->width < 2u || batch->height < 2u ||
        batch->width > 2048u || batch->height > 2048u ||
        (batch->forward.x == 0 && batch->forward.y == 0 && batch->forward.z == 0))
        return SAT_ERR_INVALID_ARG;

    batch->metrics = {};
    /* Batch preparation owns no painter ordering array: it only writes
     * face/key pairs for the final scene to sort once after merge. */
    sat_scene3d_faces_t prepared = {};
    prepared.entries=batch->faces;
    prepared.keys=batch->keys;
    prepared.capacity=batch->capacity;
    SAT_TRY(sat_scene3d_faces_begin(
        &prepared, &batch->view_proj, &batch->eye, &batch->forward,
        batch->near_depth, batch->width, batch->height));
    for (uint16_t i = 0u; i < batch->item_count; ++i) {
        const sat_scene3d_prepare_item_t& item = batch->items[i];
        if (item.instance == nullptr || item.instance->mesh == nullptr) {
            return SAT_ERR_INVALID_ARG;
        }
        const uint32_t source = static_cast<uint32_t>(batch->metrics.source_faces) +
            item.instance->mesh->face_count;
        batch->metrics.source_faces = source > 0xFFFFu
            ? 0xFFFFu : static_cast<uint16_t>(source);
        const sat_result_t st = sat_scene3d_faces_submit_instance(
            &prepared, item.instance, item.color_calc_slot,
            item.screen_scratch, item.world_scratch);
        if (st != SAT_OK) return st;
    }
    batch->metrics.prepared_faces = prepared.count;
    batch->metrics.culled_faces = prepared.culled_faces;
    batch->metrics.clipped_faces = prepared.clipped_faces;
    /* Preserve source order; the final painter sorts exactly once. */
    return SAT_OK;
}

extern "C" sat_result_t sat_scene3d_prepare_batch_slice(
    const sat_scene3d_prepare_batch_t* source,
    uint16_t first_item, uint16_t item_count,
    sat_scene3d_prepare_item_t* slice_items,
    sat_scene3d_face_t* faces, uint32_t* keys, uint16_t capacity,
    sat_scene3d_prepare_batch_t* out_slice) {
    if (source == nullptr || out_slice == nullptr ||
        (source->item_count != 0u && source->items == nullptr) ||
        first_item > source->item_count ||
        item_count > static_cast<uint16_t>(source->item_count - first_item) ||
        (item_count != 0u && slice_items == nullptr)) {
        return SAT_ERR_INVALID_ARG;
    }
    if (item_count != 0u) {
        for (uint16_t i = 0u; i < item_count; ++i) {
            slice_items[i] = source->items[first_item + i];
        }
    }
    SAT_TRY(sat_scene3d_prepare_batch_init(
        out_slice, slice_items, item_count, faces, keys, capacity));
    out_slice->view_proj = source->view_proj;
    out_slice->eye = source->eye;
    out_slice->forward = source->forward;
    out_slice->near_depth = source->near_depth;
    out_slice->width = source->width;
    out_slice->height = source->height;
    out_slice->dispatch = source->dispatch;
    return SAT_OK;
}

extern "C" sat_result_t sat_scene3d_faces_merge_prepared(
    sat_scene3d_faces_t* scene,
    const sat_scene3d_prepare_batch_t* batch) {
    if (scene == nullptr || batch == nullptr || !scene->active ||
        batch->faces == nullptr || batch->keys == nullptr ||
        batch->metrics.prepared_faces > batch->capacity)
        return SAT_ERR_INVALID_ARG;
    if (batch->metrics.prepared_faces > scene->capacity - scene->count)
        return SAT_ERR_CAPACITY;
    const uint16_t count = batch->metrics.prepared_faces;
    for (uint16_t i = 0u; i < count; ++i) {
        copy_face(scene->entries[scene->count], batch->faces[i]);
        scene->keys[scene->count] = batch->keys[i];
        ++scene->count;
    }
    scene->culled_faces = static_cast<uint16_t>(
        scene->culled_faces + batch->metrics.culled_faces);
    scene->clipped_faces = static_cast<uint16_t>(
        scene->clipped_faces + batch->metrics.clipped_faces);
    return SAT_OK;
}

extern "C" sat_result_t sat_scene3d_faces_flush(
    sat_scene3d_faces_t* scene) {
    if (!scene || !scene->active) return SAT_ERR_INVALID_ARG;
    scene->active=0u;
    /* Revalidate borrowed material residency before *any* VDP1 command.
     * A texture may have been released or marked invalid between submit and
     * flush, including in an async-prepared batch. Refuse the entire scene
     * instead of painting an invalid image over earlier valid faces. This is
     * an ordinary descriptor validity gate, NOT a VRAM ownership lock: the
     * caller must still keep valid texture storage/residency until flush. */
    /* Generation-checked owners are verified once per managed view, not per
     * face: a released or recycled owner rejects the whole scene here. */
    for (uint16_t s=0u;s<scene->owner_span_count;++s) {
        const sat_scene3d_owner_span_t& span=scene->owner_spans[s];
        if (static_cast<uint32_t>(span.first)+span.count>scene->count ||
            !scene->validate_owner ||
            scene->validate_owner(scene->owner_context,span.slot,
                span.generation,span.native)!=SAT_OK) {
            scene->count=0u;
            scene->owner_span_count=0u;
            return SAT_ERR_INVALID_ARG;
        }
    }
    scene->owner_span_count=0u;
    for (uint16_t i=0u;i<scene->count;++i) {
        if (!valid_material(scene->entries[i].material)) {
            scene->count=0u;
            return SAT_ERR_INVALID_ARG;
        }
    }
    /* Orders indices in O(faces + buckets) and never moves a face record;
     * the same helper already carries the native mesh paint order. */
#if SAT_PROFILE_METRICS
    const uint16_t painter_start=saturn::hal::sh2::frt::counter();
#endif
    /* Each pass buckets its own depth span: with one global span, two live
     * passes alone would widen every bucket to 2^20/1024 depth units. */
    const uint32_t ordered=saturn::core::render3d::paint_order_grouped_buckets(
        scene->keys,scene->count,scene->order,kFaceDepthBits);
#if SAT_PROFILE_METRICS
    g_test_painter_ticks=static_cast<uint16_t>(
        saturn::hal::sh2::frt::counter()-painter_start);
    const uint16_t emit_start=saturn::hal::sh2::frt::counter();
#endif
    sat_result_t result=SAT_OK;
    for (uint32_t i=0;i<ordered;++i) {
        if (!scene->entries[scene->order[i]].projected_safe)
            ++scene->fallback_faces;
        const sat_result_t emitted=emit(*scene,scene->entries[scene->order[i]]);
        if (emitted==SAT_ERR_UNSUPPORTED) {
            ++scene->skipped_faces;
            continue;
        }
        if (emitted!=SAT_OK) {
            result=emitted;
            break;
        }
        if (scene->entries[scene->order[i]].cached_projected != 0u)
            ++scene->emitted_cached;
        else
            ++scene->emitted_faces;
    }
#if SAT_PROFILE_METRICS
    g_test_emit_ticks=static_cast<uint16_t>(
        saturn::hal::sh2::frt::counter()-emit_start);
#endif
    scene->count=0u;
    return result;
}

#if SAT_PROFILE_METRICS
extern "C" uint32_t sat_scene3d_test_painter_ticks(void) {
    return g_test_painter_ticks;
}

extern "C" uint32_t sat_scene3d_test_emit_ticks(void) {
    return g_test_emit_ticks;
}
#endif
