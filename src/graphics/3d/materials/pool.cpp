#include "saturn/scene3d_material_pool.h"

namespace {

constexpr uint8_t kNoNode=SAT_SCENE3D_SOLID_POOL_NO_NODE;

uint8_t node_height(const sat_scene3d_solid_pool_t& pool,uint8_t node) {
    return node==kNoNode ? 0u : pool.tree_height[node];
}

void refresh_height(sat_scene3d_solid_pool_t& pool,uint8_t node) {
    const uint8_t left=node_height(pool,pool.tree_left[node]);
    const uint8_t right=node_height(pool,pool.tree_right[node]);
    pool.tree_height[node]=static_cast<uint8_t>((left>right?left:right)+1u);
}

int balance_factor(const sat_scene3d_solid_pool_t& pool,uint8_t node) {
    return static_cast<int>(node_height(pool,pool.tree_left[node]))-
           static_cast<int>(node_height(pool,pool.tree_right[node]));
}

uint8_t rotate_right(sat_scene3d_solid_pool_t& pool,uint8_t root) {
    const uint8_t child=pool.tree_left[root];
    const uint8_t middle=pool.tree_right[child];
    pool.tree_right[child]=root;
    pool.tree_left[root]=middle;
    refresh_height(pool,root);
    refresh_height(pool,child);
    return child;
}

uint8_t rotate_left(sat_scene3d_solid_pool_t& pool,uint8_t root) {
    const uint8_t child=pool.tree_right[root];
    const uint8_t middle=pool.tree_left[child];
    pool.tree_left[child]=root;
    pool.tree_right[root]=middle;
    refresh_height(pool,root);
    refresh_height(pool,child);
    return child;
}

/* AVL recursion is bounded by log2(255) after every insertion: at most a
 * handful of frames on the SH-2 stack, while keeping all metadata compact. */
uint8_t insert_node(sat_scene3d_solid_pool_t& pool,uint8_t root,uint8_t node) {
    if (root==kNoNode) return node;
    if (pool.colors[node]<pool.colors[root])
        pool.tree_left[root]=insert_node(pool,pool.tree_left[root],node);
    else
        pool.tree_right[root]=insert_node(pool,pool.tree_right[root],node);

    refresh_height(pool,root);
    const int balance=balance_factor(pool,root);
    if (balance>1) {
        const uint8_t left=pool.tree_left[root];
        if (pool.colors[node]>pool.colors[left])
            pool.tree_left[root]=rotate_left(pool,left);
        return rotate_right(pool,root);
    }
    if (balance<-1) {
        const uint8_t right=pool.tree_right[root];
        if (pool.colors[node]<pool.colors[right])
            pool.tree_right[root]=rotate_right(pool,right);
        return rotate_left(pool,root);
    }
    return root;
}

uint8_t find_exact(const sat_scene3d_solid_pool_t& pool,uint16_t rgb555) {
    uint8_t node=pool.tree_root;
    while (node!=kNoNode) {
        const uint16_t key=pool.colors[node];
        if (rgb555==key) return node;
        node=rgb555<key ? pool.tree_left[node] : pool.tree_right[node];
    }
    return kNoNode;
}

} // namespace

extern "C" sat_result_t sat_scene3d_solid_pool_init(
    sat_scene3d_solid_pool_t* pool,
    sat_scene3d_material_t* material_storage,
    sat_vdp1_texture_t* texture_storage,
    uint16_t* color_storage, uint8_t pixels[64],
    uint16_t capacity, uint16_t palette_bank) {
    if (!pool || !material_storage || !texture_storage || !color_storage ||
        !pixels || !capacity || capacity>SAT_SCENE3D_SOLID_POOL_MAX ||
        palette_bank>=8u)
        return SAT_ERR_INVALID_ARG;
    *pool={};
    pool->tree_root=kNoNode;
    pool->materials=material_storage;
    pool->textures=texture_storage;
    pool->colors=color_storage;
    pool->pixels=pixels;
    pool->capacity=capacity;
    pool->palette_bank=palette_bank;
    return SAT_OK;
}

extern "C" sat_result_t sat_scene3d_solid_pool_register(
    sat_scene3d_solid_pool_t* pool, uint16_t rgb555,
    uint16_t* out_material_index) {
    if (!pool || !pool->materials || !pool->textures || !pool->colors ||
        !pool->pixels || !out_material_index || !pool->capacity)
        return SAT_ERR_INVALID_ARG;
    const uint8_t existing=find_exact(*pool,rgb555);
    if (existing!=kNoNode) {
        *out_material_index=existing;
        return SAT_OK;
    }
    if (pool->count>=pool->capacity) return SAT_ERR_CAPACITY;
    const uint16_t index=pool->count;
    const uint8_t palette_index=static_cast<uint8_t>(index+1u);
    for (uint16_t i=0;i<64u;++i) pool->pixels[i]=palette_index;
    const sat_result_t st=sat_tex_upload_indexed8_pixels(
        &pool->textures[index],pool->pixels,8u,8u,pool->palette_bank);
    if (st!=SAT_OK) return st;
    pool->colors[index]=rgb555;
    sat_scene3d_material_t material={};
    material.kind=SAT_SCENE3D_INDEXED_SOLID;
    material.texture=&pool->textures[index];
    material.color_calc_slot=SAT_INDEXED_SOLID_OPAQUE;
    pool->materials[index]=material;
    pool->tree_left[index]=kNoNode;
    pool->tree_right[index]=kNoNode;
    pool->tree_height[index]=1u;
    pool->tree_root=insert_node(
        *pool,pool->tree_root,static_cast<uint8_t>(index));
    pool->count=static_cast<uint16_t>(index+1u);
    *out_material_index=index;
    return SAT_OK;
}

extern "C" sat_result_t sat_scene3d_solid_pool_find_nearest_in(
    const sat_scene3d_solid_pool_t* pool, uint16_t rgb555,
    uint16_t first, uint16_t count, uint16_t* out_material_index) {
    if (!pool || !pool->colors || !out_material_index)
        return SAT_ERR_INVALID_ARG;
    if (static_cast<uint32_t>(first)+count > pool->count)
        return SAT_ERR_INVALID_ARG;
    if (!count) return SAT_ERR_NOT_FOUND;
    uint16_t chosen=first;
    uint32_t best=0xFFFFFFFFu;
    for (uint16_t i=first;i<first+count;++i) {
        const uint16_t candidate=pool->colors[i];
        const int32_t dr=static_cast<int32_t>(rgb555&31u)-
                         static_cast<int32_t>(candidate&31u);
        const int32_t dg=static_cast<int32_t>((rgb555>>5u)&31u)-
                         static_cast<int32_t>((candidate>>5u)&31u);
        const int32_t db=static_cast<int32_t>((rgb555>>10u)&31u)-
                         static_cast<int32_t>((candidate>>10u)&31u);
        const uint32_t distance=static_cast<uint32_t>(dr*dr+dg*dg+db*db);
        if (distance<best) {
            best=distance;
            chosen=i;
            if (!distance) break; /* Exact match; nothing can be nearer. */
        }
    }
    *out_material_index=chosen;
    return SAT_OK;
}

extern "C" sat_result_t sat_scene3d_solid_pool_find_nearest(
    const sat_scene3d_solid_pool_t* pool, uint16_t rgb555,
    uint16_t* out_material_index) {
    if (!pool) return SAT_ERR_INVALID_ARG;
    return sat_scene3d_solid_pool_find_nearest_in(
        pool,rgb555,0u,pool->count,out_material_index);
}

extern "C" sat_result_t sat_scene3d_solid_pool_upload_palette(
    const sat_scene3d_solid_pool_t* pool) {
    if (!pool || !pool->materials || !pool->textures || !pool->colors ||
        !pool->capacity || pool->count>pool->capacity) return SAT_ERR_INVALID_ARG;
    uint16_t palette[256]={};
    for (uint16_t i=0;i<pool->count;++i) palette[i+1u]=pool->colors[i];
    return sat_palette_upload_indexed8(palette,pool->palette_bank);
}
