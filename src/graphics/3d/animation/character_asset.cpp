#include "saturn/character3d.h"
#include "saturn/asset.h"
#include <stddef.h>

namespace {
static_assert(sizeof(sat_skeleton_joint_t)==SAT_CHARACTER_JOINT_BYTES);
static_assert(sizeof(sat_skin_rigid_t)==SAT_CHARACTER_RIGID_BYTES);
static_assert(sizeof(sat_skin_blend_t)==SAT_CHARACTER_BLEND_BYTES);
static_assert(sizeof(sat_skeleton_pose_joint_t)==SAT_CHARACTER_POSE_BYTES);
static_assert(sizeof(sat_vec3_t)==SAT_CHARACTER_VERTEX_BYTES);
uint16_t be16(const uint8_t* p) { return (uint16_t)((p[0]<<8)|p[1]); }
uint32_t be32(const uint8_t* p) { return (uint32_t(be16(p))<<16)|be16(p+2); }
bool region(uint32_t off, uint32_t count, uint32_t unit, uint32_t size, uint32_t align=4) {
    return off>=SAT_CHARACTER_HEADER_BYTES && off<=size && !(off&(align-1)) && count<=(size-off)/unit;
}
bool string_at(const uint8_t* data, uint32_t offset, uint32_t size) {
    if (offset<SAT_CHARACTER_HEADER_BYTES || offset>=size) return false;
    for (uint32_t i=offset; i<size; ++i) if (!data[i]) return true;
    return false;
}
void native16(uint8_t* p, uint32_t n) {
    for (uint32_t i=0;i<n;++i) { const uint16_t v=be16(p+i*2); __builtin_memcpy(p+i*2,&v,2); }
}
void native32(uint8_t* p, uint32_t n) {
    for (uint32_t i=0;i<n;++i) { const uint32_t v=be32(p+i*4); __builtin_memcpy(p+i*4,&v,4); }
}
}

extern "C" sat_result_t sat_character_asset_decode(void* storage, uint32_t bytes,
    sat_skeleton_clip_t* clips, uint16_t clip_capacity,
    sat_model_texture_asset_t* textures, uint16_t texture_capacity,
    sat_character_asset_t* out) {
    if (!storage || !clips || !textures || !out || (uintptr_t(storage)&3u) || bytes<SAT_CHARACTER_HEADER_BYTES)
        return SAT_ERR_INVALID_ARG;
    auto* data=static_cast<uint8_t*>(storage);
    uint32_t h[SAT_CHARACTER_HEADER_WORDS]; for (uint32_t i=0;i<SAT_CHARACTER_HEADER_WORDS;++i) h[i]=be32(data+i*4);
    if (h[SAT_CHAR_MAGIC]!=SAT_CHARACTER_ARCHIVE_MAGIC || h[SAT_CHAR_VERSION]!=SAT_CHARACTER_ARCHIVE_VERSION || h[SAT_CHAR_SIZE]!=bytes || !h[SAT_CHAR_JOINTS] || h[SAT_CHAR_JOINTS]>SAT_CHARACTER_MAX_JOINTS ||
        h[SAT_CHAR_RIGID]+h[SAT_CHAR_BLEND]!=h[SAT_CHAR_VERTICES] || h[SAT_CHAR_VERTICES]>65535 || !h[SAT_CHAR_FACES] || h[SAT_CHAR_FACES]>65535 ||
        !h[SAT_CHAR_TEXTURES] || h[SAT_CHAR_TEXTURES]>65535 || h[SAT_CHAR_PALETTES]!=1 || !h[SAT_CHAR_CLIPS] || h[SAT_CHAR_CLIPS]>65535 ||
        h[SAT_CHAR_POS_SHIFT]>SAT_CHARACTER_MAX_POS_SHIFT || h[SAT_CHAR_MAX_JOINT]>=h[SAT_CHAR_JOINTS]) return SAT_ERR_INVALID_ARG;
    if (h[SAT_CHAR_TEXTURES]>texture_capacity || h[SAT_CHAR_CLIPS]>clip_capacity) return SAT_ERR_CAPACITY;
    const uint32_t units[SAT_CHAR_TABLE_COUNT]={SAT_CHARACTER_JOINT_BYTES,SAT_CHARACTER_RIGID_BYTES,SAT_CHARACTER_BLEND_BYTES,SAT_CHARACTER_VERTEX_BYTES,SAT_CHARACTER_FACE_BYTES,sizeof(uint16_t),SAT_CHARACTER_TEXTURE_BYTES,SAT_CHARACTER_PALETTE_BYTES,SAT_CHARACTER_CLIP_BYTES,sizeof(uint32_t),2*SAT_CHARACTER_VERTEX_BYTES};
    const uint32_t counts[SAT_CHAR_TABLE_COUNT]={h[SAT_CHAR_JOINTS],h[SAT_CHAR_RIGID],h[SAT_CHAR_BLEND],h[SAT_CHAR_VERTICES],h[SAT_CHAR_FACES],h[SAT_CHAR_FACES],h[SAT_CHAR_TEXTURES],h[SAT_CHAR_PALETTES],h[SAT_CHAR_CLIPS],h[SAT_CHAR_JOINTS],1};
    for (uint32_t i=0;i<SAT_CHAR_TABLE_COUNT;++i) {
        if (!region(h[SAT_CHAR_JOINT_OFFSET+i],counts[i],units[i],bytes,i==2||i==5?2:4)) return SAT_ERR_INVALID_ARG;
        // Fixed tables may not overlap one another or the header.
        for (uint32_t j=0;j<i;++j)
            if (counts[i] && counts[j] && h[SAT_CHAR_JOINT_OFFSET+i]<h[SAT_CHAR_JOINT_OFFSET+j]+counts[j]*units[j] &&
                h[SAT_CHAR_JOINT_OFFSET+j]<h[SAT_CHAR_JOINT_OFFSET+i]+counts[i]*units[i]) return SAT_ERR_INVALID_ARG;
    }
    const auto overlaps_fixed=[&](uint32_t offset,uint32_t length) {
        for (uint32_t table=0;table<SAT_CHAR_TABLE_COUNT;++table)
            if (counts[table] && offset<h[SAT_CHAR_JOINT_OFFSET+table]+counts[table]*units[table] &&
                h[SAT_CHAR_JOINT_OFFSET+table]<offset+length) return true;
        return false;
    };
    for (uint32_t j=0;j<h[SAT_CHAR_JOINTS];++j) {
        const uint16_t parent=be16(data+h[SAT_CHAR_JOINT_OFFSET]+j*SAT_CHARACTER_JOINT_BYTES);
        if (parent!=SAT_TRANSFORM3D_ROOT && parent>=j) return SAT_ERR_INVALID_ARG;
        if (!string_at(data,be32(data+h[SAT_CHAR_NAME_OFFSET]+j*sizeof(uint32_t)),bytes)) return SAT_ERR_INVALID_ARG;
    }
    for (uint32_t v=0;v<h[SAT_CHAR_RIGID];++v) if (data[h[SAT_CHAR_RIGID_OFFSET]+v*SAT_CHARACTER_RIGID_BYTES+6]>=h[SAT_CHAR_JOINTS]) return SAT_ERR_INVALID_ARG;
    for (uint32_t v=0;v<h[SAT_CHAR_BLEND];++v) {
        const uint8_t* p=data+h[SAT_CHAR_BLEND_OFFSET]+v*SAT_CHARACTER_BLEND_BYTES;
        if (p[3]<2 || p[3]>3 || be16(p+4)>32768 ||
            (p[3]==3 && uint32_t(be16(p+4))+be16(p+6)>32768)) return SAT_ERR_INVALID_ARG;
        for (uint8_t k=0;k<p[3];++k) if (p[k]>=h[SAT_CHAR_JOINTS]) return SAT_ERR_INVALID_ARG;
    }
    for (uint32_t f=0;f<h[SAT_CHAR_FACES];++f) {
        for (uint32_t c=0;c<4;++c) if (be16(data+h[SAT_CHAR_FACE_OFFSET]+f*SAT_CHARACTER_FACE_BYTES+c*2)>=h[SAT_CHAR_VERTICES]) return SAT_ERR_INVALID_ARG;
        if (be16(data+h[SAT_CHAR_SELECTOR_OFFSET]+f*sizeof(uint16_t))>=h[SAT_CHAR_TEXTURES]) return SAT_ERR_INVALID_ARG;
    }
    for (uint32_t i=0;i<h[SAT_CHAR_TEXTURES];++i) {
        const uint8_t* t=data+h[SAT_CHAR_TEXTURE_OFFSET]+i*SAT_CHARACTER_TEXTURE_BYTES;
        const uint32_t w=be16(t+SAT_CHAR_TEXTURE_WIDTH), height=be16(t+SAT_CHAR_TEXTURE_HEIGHT), n=be32(t+SAT_CHAR_TEXTURE_PIXEL_COUNT);
        if (be16(t+SAT_CHAR_TEXTURE_PALETTE_BASE)>7 ||
            be16(t+SAT_CHAR_TEXTURE_PALETTE_BASE)!=be16(data+h[SAT_CHAR_TEXTURE_OFFSET]+SAT_CHAR_TEXTURE_PALETTE_BASE))
            return SAT_ERR_INVALID_ARG;
        if (!w || w>504 || w%8 || !height || height>255 || n!=w*height ||
            !region(be32(t),n,1,bytes,8)) return SAT_ERR_INVALID_ARG;
        if (overlaps_fixed(be32(t),n)) return SAT_ERR_INVALID_ARG;
    }
    for (uint32_t i=0;i<h[SAT_CHAR_CLIPS];++i) {
        const uint8_t* c=data+h[SAT_CHAR_CLIP_OFFSET]+i*SAT_CHARACTER_CLIP_BYTES;
        const uint32_t frames=be16(c+SAT_CHAR_CLIP_FRAME_COUNT), joints=be16(c+SAT_CHAR_CLIP_JOINT_COUNT), events=be16(c+SAT_CHAR_CLIP_EVENT_COUNT);
        if (!frames || joints!=h[SAT_CHAR_JOINTS] || !be16(c+SAT_CHAR_CLIP_RATE_NUM) || !be16(c+SAT_CHAR_CLIP_RATE_DEN) ||
            !region(be32(c),frames*joints,SAT_CHARACTER_POSE_BYTES,bytes) ||
            (events && !region(be32(c+SAT_CHAR_CLIP_EVENTS),events,4,bytes,2)) ||
            !string_at(data,be32(c+SAT_CHAR_CLIP_NAME),bytes)) return SAT_ERR_INVALID_ARG;
        const uint32_t pose_offset=be32(c),pose_bytes=frames*joints*SAT_CHARACTER_POSE_BYTES;
        const uint32_t event_offset=be32(c+SAT_CHAR_CLIP_EVENTS),event_bytes=events*SAT_CHARACTER_EVENT_BYTES;
        if (overlaps_fixed(pose_offset,pose_bytes) || (events && overlaps_fixed(event_offset,event_bytes)))
            return SAT_ERR_INVALID_ARG;
        if (events && pose_offset<event_offset+event_bytes && event_offset<pose_offset+pose_bytes)
            return SAT_ERR_INVALID_ARG;
        for (uint32_t previous=0;previous<i;++previous) {
            const uint8_t* old=data+h[SAT_CHAR_CLIP_OFFSET]+previous*SAT_CHARACTER_CLIP_BYTES;
            const uint32_t old_pose=be32(old),old_bytes=be16(old+SAT_CHAR_CLIP_FRAME_COUNT)*joints*SAT_CHARACTER_POSE_BYTES;
            const uint32_t old_events=be32(old+SAT_CHAR_CLIP_EVENTS),old_event_bytes=be16(old+SAT_CHAR_CLIP_EVENT_COUNT)*SAT_CHARACTER_EVENT_BYTES;
            if ((pose_offset<old_pose+old_bytes && old_pose<pose_offset+pose_bytes) ||
                (events && event_offset<old_pose+old_bytes && old_pose<event_offset+event_bytes) ||
                (old_event_bytes && pose_offset<old_events+old_event_bytes && old_events<pose_offset+pose_bytes) ||
                (events && old_event_bytes && event_offset<old_events+old_event_bytes && old_events<event_offset+event_bytes))
                return SAT_ERR_INVALID_ARG;
        }
        for (uint32_t tex=0;tex<h[SAT_CHAR_TEXTURES];++tex) {
            const uint8_t* t=data+h[SAT_CHAR_TEXTURE_OFFSET]+tex*SAT_CHARACTER_TEXTURE_BYTES;
            const uint32_t start=be32(t),length=be32(t+SAT_CHAR_TEXTURE_PIXEL_COUNT);
            if ((pose_offset<start+length && start<pose_offset+pose_bytes) ||
                (events && event_offset<start+length && start<event_offset+event_bytes)) return SAT_ERR_INVALID_ARG;
        }
        uint16_t last=0;
        for (uint32_t e=0;e<events;++e) {
            const uint16_t frame=be16(data+be32(c+SAT_CHAR_CLIP_EVENTS)+e*4);
            if (frame>=frames || (e && frame<last)) return SAT_ERR_INVALID_ARG;
            last=frame;
        }
    }
    // All bounds/capacities have passed before converting any payload table.
    sat_character_asset_t result{};
    for (uint32_t i=0;i<h[SAT_CHAR_JOINTS];++i) {
        uint8_t* p=data+h[SAT_CHAR_JOINT_OFFSET]+i*SAT_CHARACTER_JOINT_BYTES;
        native16(p,2); native32(p+offsetof(sat_skeleton_joint_t,rest_translation),3); native16(p+offsetof(sat_skeleton_joint_t,rest_rotation),4); native32(p+offsetof(sat_skeleton_joint_t,rest_scale),3);
    }
    for (uint32_t i=0;i<h[SAT_CHAR_RIGID];++i) native16(data+h[SAT_CHAR_RIGID_OFFSET]+i*8,3);
    for (uint32_t i=0;i<h[SAT_CHAR_BLEND];++i) native16(data+h[SAT_CHAR_BLEND_OFFSET]+i*SAT_CHARACTER_BLEND_BYTES+4,11);
    native32(data+h[SAT_CHAR_VERTEX_OFFSET],h[SAT_CHAR_VERTICES]*sizeof(sat_vec3_t)/sizeof(uint32_t)); native16(data+h[SAT_CHAR_FACE_OFFSET],h[SAT_CHAR_FACES]*4);
    native16(data+h[SAT_CHAR_SELECTOR_OFFSET],h[SAT_CHAR_FACES]); native16(data+h[SAT_CHAR_PALETTE_OFFSET],256);
    native32(data+h[SAT_CHAR_NAME_OFFSET],h[SAT_CHAR_JOINTS]); native32(data+h[SAT_CHAR_BOUNDS_OFFSET],6);
    result.skeleton={reinterpret_cast<const sat_skeleton_joint_t*>(data+h[SAT_CHAR_JOINT_OFFSET]),(uint16_t)h[SAT_CHAR_JOINTS],0};
    result.skin={reinterpret_cast<const sat_skin_rigid_t*>(data+h[SAT_CHAR_RIGID_OFFSET]),
        reinterpret_cast<const sat_skin_blend_t*>(data+h[SAT_CHAR_BLEND_OFFSET]),nullptr,
        (uint16_t)h[SAT_CHAR_RIGID],(uint16_t)h[SAT_CHAR_BLEND],(uint16_t)h[SAT_CHAR_JOINTS],(uint8_t)h[SAT_CHAR_POS_SHIFT],(uint8_t)h[SAT_CHAR_MAX_JOINT]};
    for (uint32_t i=0;i<h[SAT_CHAR_TEXTURES];++i) {
        const uint8_t* t=data+h[SAT_CHAR_TEXTURE_OFFSET]+i*SAT_CHARACTER_TEXTURE_BYTES;
        textures[i]={data+be32(t),be16(t+SAT_CHAR_TEXTURE_WIDTH),be16(t+SAT_CHAR_TEXTURE_HEIGHT),0,be16(t+SAT_CHAR_TEXTURE_FLAGS),be32(t+SAT_CHAR_TEXTURE_PIXEL_COUNT)};
    }
    for (uint32_t i=0;i<h[SAT_CHAR_CLIPS];++i) {
        const uint8_t* c=data+h[SAT_CHAR_CLIP_OFFSET]+i*SAT_CHARACTER_CLIP_BYTES;
        const uint32_t off=be32(c), ev=be32(c+SAT_CHAR_CLIP_EVENTS), nf=be16(c+SAT_CHAR_CLIP_FRAME_COUNT), ne=be16(c+SAT_CHAR_CLIP_EVENT_COUNT);
        for (uint32_t p=0;p<nf*h[SAT_CHAR_JOINTS];++p) {
            native32(data+off+p*SAT_CHARACTER_POSE_BYTES,3); native16(data+off+p*SAT_CHARACTER_POSE_BYTES+offsetof(sat_skeleton_pose_joint_t,rotation),4);
        }
        if (ne) native16(data+ev,ne*2);
        clips[i]={reinterpret_cast<const sat_skeleton_pose_joint_t*>(data+off),
            ne?reinterpret_cast<const sat_clip_event_t*>(data+ev):nullptr,
            (uint16_t)nf,(uint16_t)h[SAT_CHAR_JOINTS],(uint16_t)ne,be16(c+SAT_CHAR_CLIP_RATE_NUM),be16(c+SAT_CHAR_CLIP_RATE_DEN),be16(c+SAT_CHAR_CLIP_FLAGS)};
    }
    result.model={reinterpret_cast<const sat_vec3_t*>(data+h[SAT_CHAR_VERTEX_OFFSET]),(uint16_t)h[SAT_CHAR_VERTICES],
        reinterpret_cast<const uint16_t*>(data+h[SAT_CHAR_FACE_OFFSET]),(uint16_t)h[SAT_CHAR_FACES],
        reinterpret_cast<const uint16_t*>(data+h[SAT_CHAR_SELECTOR_OFFSET]),textures,(uint16_t)h[SAT_CHAR_TEXTURES],
        reinterpret_cast<const uint16_t*>(data+h[SAT_CHAR_PALETTE_OFFSET]),1,
        be16(data+h[SAT_CHAR_TEXTURE_OFFSET]+SAT_CHAR_TEXTURE_PALETTE_BASE),0,nullptr,0,nullptr,nullptr,0};
    result.clips=clips; result.clip_count=(uint16_t)h[SAT_CHAR_CLIPS]; result.storage=data;
    result.storage_bytes=bytes; result.joint_names=reinterpret_cast<const uint32_t*>(data+h[SAT_CHAR_NAME_OFFSET]);
    result.clip_directory=data+h[SAT_CHAR_CLIP_OFFSET];
    __builtin_memcpy(&result.bind_min,data+h[SAT_CHAR_BOUNDS_OFFSET],sizeof(sat_vec3_t)); __builtin_memcpy(&result.bind_max,data+h[SAT_CHAR_BOUNDS_OFFSET]+sizeof(sat_vec3_t),sizeof(sat_vec3_t));
    SAT_TRY(sat_skeleton_validate(&result.skeleton));
    SAT_TRY(sat_skin_validate(&result.skin));
    for (uint16_t i=0;i<result.clip_count;++i) SAT_TRY(sat_clip_validate(&clips[i],&result.skeleton));
    *out=result;
    return SAT_OK;
}

extern "C" sat_result_t sat_character_asset_load(const char* path,
    void* storage, uint32_t capacity, sat_skeleton_clip_t* clips, uint16_t clip_capacity,
    sat_model_texture_asset_t* textures, uint16_t texture_capacity, sat_character_asset_t* out) {
    if (!path || !storage || !out) return SAT_ERR_INVALID_ARG;
    sat_asset_t handle; SAT_TRY(sat_asset_open(path,&handle));
    sat_asset_info_t info{}; const sat_result_t known=sat_asset_info(handle,&info);
    if (known!=SAT_OK) return known;
    if (info.size>capacity) return SAT_ERR_CAPACITY;
    uint32_t read=0; SAT_TRY(sat_asset_read_at(path,0,storage,info.size,&read));
    if (read!=info.size) return SAT_ERR_IO;
    return sat_character_asset_decode(storage,read,clips,clip_capacity,textures,texture_capacity,out);
}
extern "C" void sat_character_asset_release(sat_character_asset_t* asset) {
    if (asset) *asset={};
}

extern "C" sat_result_t sat_character_asset_compact(sat_character_asset_t* asset,
    void* storage,uint32_t capacity,uint32_t* out_bytes) {
    if (!asset || !asset->storage || !storage || (reinterpret_cast<uintptr_t>(storage)&3u) ||
        !asset->model.texture_count || !asset->model.textures) return SAT_ERR_INVALID_ARG;
    const uint8_t* source=asset->storage;
    uint8_t* target=static_cast<uint8_t*>(storage);
    const uint32_t bytes=asset->storage_bytes;
    if (target<source+bytes && source<target+capacity) return SAT_ERR_INVALID_ARG;
    uint32_t first=UINT32_MAX,last=0;
    for (uint16_t i=0;i<asset->model.texture_count;++i) {
        const sat_model_texture_asset_t& t=asset->model.textures[i];
        const uint32_t size=t.flags&SAT_MODEL_TEXTURE_LUT4?t.pixel_count/2u:t.pixel_count;
        const uint32_t at=(uint32_t)(t.pixels-source);
        if (at<first) first=at;
        if (at+size>last) last=at+size;
    }
    if (last>bytes) return SAT_ERR_INVALID_ARG;
    /* The tables sit before the pixels; the clips and their names after them.
     * The texture table goes with the pixels it describes. */
    const uint32_t hole_at=be32(source+SAT_CHAR_TEXTURE_OFFSET*4u);
    const uint32_t hole_bytes=(uint32_t)asset->model.texture_count*SAT_CHARACTER_TEXTURE_BYTES;
    if (hole_at&3u || hole_at>first || hole_bytes>first-hole_at) return SAT_ERR_INVALID_ARG;
    const uint32_t hole_end=hole_at+hole_bytes;
    const uint32_t kept=((first+3u)&~3u)-hole_bytes;
    if (kept+hole_bytes>last) return SAT_ERR_INVALID_ARG;
    const uint32_t tail_at=(last+3u)&~3u;
    const uint32_t tail_bytes=tail_at<bytes?bytes-tail_at:0u;
    if (kept+tail_bytes>capacity) return SAT_ERR_CAPACITY;
    const auto inside_pixels=[&](const void* p) {
        if (!p) return false;
        const uint32_t at=(uint32_t)(static_cast<const uint8_t*>(p)-source);
        return at>=first && at<tail_at;
    };
    for (uint16_t i=0;i<asset->clip_count;++i)
        if (inside_pixels(asset->clips[i].frames) || inside_pixels(asset->clips[i].events)) return SAT_ERR_INVALID_ARG;
    __builtin_memcpy(target,source,hole_at);
    __builtin_memcpy(target+hole_at,source+hole_end,first-hole_end);
    if (tail_bytes) __builtin_memcpy(target+kept,source+tail_at,tail_bytes);
    const auto new_offset=[&](uint32_t at) {
        return at<hole_at?at:at<first?at-hole_bytes:at-tail_at+kept;
    };
    const auto moved=[&](const void* p)->const uint8_t* {
        if (!p) return nullptr;
        return target+new_offset((uint32_t)(static_cast<const uint8_t*>(p)-source));
    };
    const auto rebase_wire=[&](uint8_t* field) {
        const uint32_t now=new_offset(be32(field));
        field[0]=(uint8_t)(now>>24);field[1]=(uint8_t)(now>>16);field[2]=(uint8_t)(now>>8);field[3]=(uint8_t)now;
    };
    uint8_t* directory=const_cast<uint8_t*>(moved(asset->clip_directory));
    for (uint16_t i=0;i<asset->clip_count;++i) {
        sat_skeleton_clip_t& c=asset->clips[i];
        c.frames=reinterpret_cast<const sat_skeleton_pose_joint_t*>(moved(c.frames));
        c.events=reinterpret_cast<const sat_clip_event_t*>(moved(c.events));
        uint8_t* row=directory+i*SAT_CHARACTER_CLIP_BYTES;
        rebase_wire(row);rebase_wire(row+SAT_CHAR_CLIP_EVENTS);rebase_wire(row+SAT_CHAR_CLIP_NAME);
    }
    asset->skeleton.joints=reinterpret_cast<const sat_skeleton_joint_t*>(moved(asset->skeleton.joints));
    asset->skin.rigid=reinterpret_cast<const sat_skin_rigid_t*>(moved(asset->skin.rigid));
    asset->skin.blend=reinterpret_cast<const sat_skin_blend_t*>(moved(asset->skin.blend));
    asset->model.vertices=reinterpret_cast<const sat_vec3_t*>(moved(asset->model.vertices));
    asset->model.indices=reinterpret_cast<const uint16_t*>(moved(asset->model.indices));
    asset->model.face_texture_indices=reinterpret_cast<const uint16_t*>(moved(asset->model.face_texture_indices));
    asset->model.palettes_rgb555=reinterpret_cast<const uint16_t*>(moved(asset->model.palettes_rgb555));
    asset->model.textures=nullptr;
    asset->joint_names=reinterpret_cast<const uint32_t*>(moved(asset->joint_names));
    for (uint16_t i=0;i<asset->skeleton.joint_count;++i) /* native since the decode */
        const_cast<uint32_t*>(asset->joint_names)[i]=new_offset(asset->joint_names[i]);
    asset->clip_directory=directory;
    asset->storage=target;
    asset->storage_bytes=kept+tail_bytes;
    if (out_bytes) *out_bytes=asset->storage_bytes;
    return SAT_OK;
}

extern "C" sat_result_t sat_character_clip_load(const char* path,uint16_t index,
    sat_skeleton_pose_joint_t* poses,uint32_t pose_capacity,
    sat_clip_event_t* events,uint16_t event_capacity,sat_skeleton_clip_t* out) {
    if (!path || !poses || !out) return SAT_ERR_INVALID_ARG;
    uint8_t header[SAT_CHARACTER_HEADER_BYTES];uint32_t read=0;
    SAT_TRY(sat_asset_read_at(path,0,header,sizeof(header),&read));
    if (read!=sizeof(header) || be32(header)!=SAT_CHARACTER_ARCHIVE_MAGIC ||
        be32(header+sizeof(uint32_t))!=SAT_CHARACTER_ARCHIVE_VERSION) return SAT_ERR_INVALID_ARG;
    const uint32_t size=be32(header+SAT_CHAR_SIZE*sizeof(uint32_t));
    const uint32_t count=be32(header+SAT_CHAR_CLIPS*sizeof(uint32_t));
    const uint32_t directory=be32(header+SAT_CHAR_CLIP_OFFSET*sizeof(uint32_t));
    if (index>=count || !region(directory,count,SAT_CHARACTER_CLIP_BYTES,size)) return SAT_ERR_INVALID_ARG;
    uint8_t meta[SAT_CHARACTER_CLIP_BYTES];
    SAT_TRY(sat_asset_read_at(path,directory+index*SAT_CHARACTER_CLIP_BYTES,meta,sizeof(meta),&read));
    if (read!=sizeof(meta)) return SAT_ERR_IO;
    const uint32_t frame_count=be16(meta+SAT_CHAR_CLIP_FRAME_COUNT),joints=be16(meta+SAT_CHAR_CLIP_JOINT_COUNT),event_count=be16(meta+SAT_CHAR_CLIP_EVENT_COUNT);
    const uint32_t total=frame_count*joints,offset=be32(meta),event_offset=be32(meta+SAT_CHAR_CLIP_EVENTS);
    if (!frame_count || !joints || joints>SAT_CHARACTER_MAX_JOINTS || !be16(meta+SAT_CHAR_CLIP_RATE_NUM) || !be16(meta+SAT_CHAR_CLIP_RATE_DEN) ||
        !region(offset,total,SAT_CHARACTER_POSE_BYTES,size) ||
        (event_count && (!events || !region(event_offset,event_count,SAT_CHARACTER_EVENT_BYTES,size,2))))
        return SAT_ERR_INVALID_ARG;
    if (total>pose_capacity || event_count>event_capacity) return SAT_ERR_CAPACITY;
    SAT_TRY(sat_asset_read_at(path,offset,poses,total*SAT_CHARACTER_POSE_BYTES,&read));
    if (read!=total*SAT_CHARACTER_POSE_BYTES) return SAT_ERR_IO;
    auto* data=reinterpret_cast<uint8_t*>(poses);
    for (uint32_t p=0;p<total;++p) {
        native32(data+p*SAT_CHARACTER_POSE_BYTES,3);
        native16(data+p*SAT_CHARACTER_POSE_BYTES+offsetof(sat_skeleton_pose_joint_t,rotation),4);
    }
    if (event_count) {
        SAT_TRY(sat_asset_read_at(path,event_offset,events,event_count*SAT_CHARACTER_EVENT_BYTES,&read));
        if (read!=event_count*SAT_CHARACTER_EVENT_BYTES) return SAT_ERR_IO;
        native16(reinterpret_cast<uint8_t*>(events),event_count*2);
    }
    sat_skeleton_clip_t result{poses,event_count?events:nullptr,(uint16_t)frame_count,
        (uint16_t)joints,(uint16_t)event_count,be16(meta+SAT_CHAR_CLIP_RATE_NUM),be16(meta+SAT_CHAR_CLIP_RATE_DEN),be16(meta+SAT_CHAR_CLIP_FLAGS)};
    SAT_TRY(sat_clip_validate(&result,nullptr));
    *out=result;
    return SAT_OK;
}
extern "C" const char* sat_character_joint_name(const sat_character_asset_t* asset,uint16_t joint) {
    if (!asset || !asset->storage || joint>=asset->skeleton.joint_count) return nullptr;
    return reinterpret_cast<const char*>(asset->storage+asset->joint_names[joint]);
}
extern "C" const char* sat_character_clip_name(const sat_character_asset_t* asset,uint16_t clip) {
    if (!asset || !asset->storage || clip>=asset->clip_count) return nullptr;
    return reinterpret_cast<const char*>(asset->storage+be32(asset->clip_directory+clip*SAT_CHARACTER_CLIP_BYTES+SAT_CHAR_CLIP_NAME));
}
