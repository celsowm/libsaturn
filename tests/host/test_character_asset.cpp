#include "saturn/character3d.h"
#include "saturn/asset.h"
#include <vector>
#include <fstream>
#include <iterator>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <string>
#include <algorithm>

static std::vector<uint8_t> payload;
extern "C" sat_result_t sat_asset_open(const char*,sat_asset_t* out) { *out={0,1};return SAT_OK; }
extern "C" sat_result_t sat_asset_info(sat_asset_t,sat_asset_info_t* out) { *out={};out->size=(uint32_t)payload.size();return SAT_OK; }
extern "C" sat_result_t sat_asset_read_at(const char*,uint32_t offset,void* out,uint32_t count,uint32_t* read) {
    if(offset>payload.size() || count>payload.size()-offset)return SAT_ERR_INVALID_ARG;
    std::memcpy(out,payload.data()+offset,count);*read=count;return SAT_OK;
}
static void load_fixture(const char* path) {
    std::ifstream file(path,std::ios::binary);assert(file.good());
    payload.assign(std::istreambuf_iterator<char>(file),{});
}
static void write32(std::vector<uint32_t>& data,unsigned field,uint32_t v) {
    auto* p=(uint8_t*)data.data()+field*sizeof(uint32_t);
    p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;
}
static uint32_t fixture_count(unsigned field) {
    const auto* p=payload.data()+field*sizeof(uint32_t);
    return (uint32_t)p[0]<<24 | (uint32_t)p[1]<<16 | (uint32_t)p[2]<<8 | p[3];
}
int main() {
    load_fixture("build/generated/character_examples/disc/high_blue.ska");
    const unsigned ClipCapacity=fixture_count(SAT_CHAR_CLIPS);
    const unsigned TextureCapacity=fixture_count(SAT_CHAR_TEXTURES);
    std::vector<uint32_t> blue((payload.size()+sizeof(uint32_t)-1)/sizeof(uint32_t));
    auto red=blue;
    std::vector<sat_skeleton_clip_t> clips[2]={std::vector<sat_skeleton_clip_t>(ClipCapacity),std::vector<sat_skeleton_clip_t>(ClipCapacity)};
    std::vector<sat_model_texture_asset_t> textures[2]={std::vector<sat_model_texture_asset_t>(TextureCapacity),std::vector<sat_model_texture_asset_t>(TextureCapacity)};
    sat_character_asset_t assets[2]{};
    assert(sat_character_asset_load("blue",blue.data(),(uint32_t)(blue.size()*sizeof(uint32_t)),
        clips[0].data(),ClipCapacity,textures[0].data(),TextureCapacity,&assets[0])==SAT_OK);
    assert(assets[0].storage_bytes>SAT_ASSET_CACHE_BLOCK_BYTES);
    assert(std::strcmp(sat_character_joint_name(&assets[0],0),"root")==0);
    assert(std::strcmp(sat_character_clip_name(&assets[0],4),"punch")==0);
    assert(assets[0].clips[4].event_count==1);
    assert(assets[0].clips[1].flags&SAT_CLIP_ROOT_Z);
    std::vector<sat_skeleton_pose_joint_t> selected(assets[0].clips[4].frame_count*assets[0].skeleton.joint_count);
    sat_clip_event_t events[4];sat_skeleton_clip_t selected_clip{};
    assert(sat_character_clip_load("blue",4,selected.data(),(uint32_t)selected.size(),events,4,&selected_clip)==SAT_OK);
    assert(std::memcmp(selected.data(),assets[0].clips[4].frames,selected.size()*sizeof(selected[0]))==0);
    assert(selected_clip.event_count==1 && events[0].id==1);
    load_fixture("build/generated/character_examples/disc/high_red.ska");
    assert(sat_character_asset_load("red",red.data(),(uint32_t)(red.size()*sizeof(uint32_t)),
        clips[1].data(),ClipCapacity,textures[1].data(),TextureCapacity,&assets[1])==SAT_OK);
    assert(assets[0].skin.rigid!=assets[1].skin.rigid);
    assert(assets[0].model.palettes_rgb555[1]!=assets[1].model.palettes_rgb555[1]);
    std::vector<sat_skeleton_pose_joint_t> pose(assets[0].skeleton.joint_count);
    assert(sat_clip_sample(&assets[0].clips[4],SAT_FX16_ONE/4,pose.data(),(uint16_t)pose.size())==SAT_OK);
    sat_character_asset_release(&assets[0]);assert(!assets[0].storage);
    assert(sat_clip_sample(&assets[1].clips[1],SAT_FX16_ONE/3,pose.data(),(uint16_t)pose.size())==SAT_OK);
    assert(!sat_character_joint_name(&assets[0],0));
    {
        /* Compacting keeps every view readable and drops only the pixels and their table. */
        const sat_skeleton_joint_t first_joint=assets[1].skeleton.joints[0];
        const sat_vec3_t first_vertex=assets[1].model.vertices[0];
        const uint16_t first_face=assets[1].model.indices[0],palette=assets[1].model.palettes_rgb555[1];
        std::vector<sat_skeleton_pose_joint_t> before(assets[1].clips[1].frame_count*assets[1].skeleton.joint_count);
        std::memcpy(before.data(),assets[1].clips[1].frames,before.size()*sizeof(before[0]));
        const std::string clip_name=sat_character_clip_name(&assets[1],4);
        const uint32_t full=assets[1].storage_bytes;
        std::vector<uint32_t> small(full/sizeof(uint32_t)+1);
        uint32_t retained=0;
        assert(sat_character_asset_compact(&assets[1],small.data(),8,0,&retained)==SAT_ERR_CAPACITY);
        assert(sat_character_asset_compact(&assets[1],small.data(),
            (uint32_t)(small.size()*sizeof(uint32_t)),0,&retained)==SAT_OK);
        assert(retained<full && assets[1].storage==(const uint8_t*)small.data());
        std::fill(red.begin(),red.end(),0xA5A5A5A5u);
        assert(!assets[1].model.textures);
        assert(std::memcmp(&assets[1].skeleton.joints[0],&first_joint,sizeof(first_joint))==0);
        assert(std::memcmp(&assets[1].model.vertices[0],&first_vertex,sizeof(first_vertex))==0);
        assert(assets[1].model.indices[0]==first_face && assets[1].model.palettes_rgb555[1]==palette);
        assert(std::memcmp(assets[1].clips[1].frames,before.data(),before.size()*sizeof(before[0]))==0);
        assert(clip_name==sat_character_clip_name(&assets[1],4));
        assert(std::strcmp(sat_character_joint_name(&assets[1],0),"root")==0);
        assert(sat_skin_validate(&assets[1].skin)==SAT_OK);
        assert(sat_clip_sample(&assets[1].clips[1],SAT_FX16_ONE/3,pose.data(),(uint16_t)pose.size())==SAT_OK);
    }
    load_fixture("build/generated/character_examples/disc/high_blue.ska");
    std::memcpy(blue.data(),payload.data(),payload.size());
    sat_character_asset_t sentinel{};sentinel.storage_bytes=123;
    assert(sat_character_asset_decode(blue.data(),(uint32_t)payload.size(),clips[0].data(),1,
        textures[0].data(),TextureCapacity,&sentinel)==SAT_ERR_CAPACITY && sentinel.storage_bytes==123);
    assert(std::memcmp(blue.data(),payload.data(),payload.size())==0);
    write32(blue,SAT_CHAR_FACE_OFFSET,UINT32_MAX);
    assert(sat_character_asset_decode(blue.data(),(uint32_t)payload.size(),clips[0].data(),ClipCapacity,
        textures[0].data(),TextureCapacity,&sentinel)==SAT_ERR_INVALID_ARG && sentinel.storage_bytes==123);
    std::memcpy(blue.data(),payload.data(),payload.size());write32(blue,SAT_CHAR_JOINTS,UINT32_MAX);
    assert(sat_character_asset_decode(blue.data(),(uint32_t)payload.size(),clips[0].data(),ClipCapacity,
        textures[0].data(),TextureCapacity,&sentinel)==SAT_ERR_INVALID_ARG);
    assert(sat_character_asset_load("blue",blue.data(),SAT_CHARACTER_HEADER_BYTES,
        clips[0].data(),ClipCapacity,textures[0].data(),TextureCapacity,&sentinel)==SAT_ERR_CAPACITY);
    {
        /* Version 2: LUT4 textures index the 16-entry tables the palette table holds. */
        load_fixture("build/generated/character_examples/disc/high_lut4.ska");
        std::vector<uint32_t> lut((payload.size()+3)/4),small(payload.size()/4+1);
        std::memcpy(lut.data(),payload.data(),payload.size());
        assert(fixture_count(SAT_CHAR_VERSION)==SAT_CHARACTER_ARCHIVE_VERSION_LUT4);
        sat_character_asset_t asset{};
        assert(sat_character_asset_decode(lut.data(),(uint32_t)payload.size(),clips[0].data(),ClipCapacity,
            textures[0].data(),TextureCapacity,&asset)==SAT_OK);
        const sat_model_asset_t& m=asset.model;
        assert(!m.palette_count && !m.palettes_rgb555 && m.luts_rgb555 && m.lut_count>=m.texture_count);
        for(uint16_t t=0;t<m.texture_count;++t)
            assert((m.textures[t].flags&SAT_MODEL_TEXTURE_LUT4) && m.textures[t].palette_slot==t);
        assert(sat_model_validate(&m)==SAT_OK);
        /* Compacting drops the colour tables with the pixels and keeps the rest readable. */
        const uint32_t lut_bytes=m.lut_count*16u*(uint32_t)sizeof(uint16_t);
        const uint32_t texture_bytes=m.texture_count*(uint32_t)SAT_CHARACTER_TEXTURE_BYTES;
        const sat_vec3_t first_vertex=m.vertices[0];
        const std::string clip_name=sat_character_clip_name(&asset,1);
        /* Compaction rebases the clips in place: each copy of the view takes its own. */
        sat_character_asset_t plain=asset,bare=asset;
        std::vector<sat_skeleton_clip_t> plain_clips(asset.clips,asset.clips+asset.clip_count),bare_clips=plain_clips;
        plain.clips=plain_clips.data();bare.clips=bare_clips.data();
        std::vector<uint32_t> with_luts(payload.size()/4+1),without_vertices(payload.size()/4+1);
        uint32_t kept=0,kept_plain=0,kept_bare=0;
        /* A table that does not follow the texture table stays. */
        plain.model.luts_rgb555=nullptr;plain.model.lut_count=0;
        assert(sat_character_asset_compact(&plain,with_luts.data(),(uint32_t)(with_luts.size()*4),0,&kept_plain)==SAT_OK);
        /* The bind vertices go on request; every other view stays readable. */
        const uint16_t first_face=m.indices[0],last_selector=m.face_texture_indices[m.face_count-1u];
        const sat_skeleton_joint_t first_joint=asset.skeleton.joints[0];
        assert(sat_character_asset_compact(&bare,without_vertices.data(),(uint32_t)(without_vertices.size()*4),
            SAT_CHARACTER_COMPACT_VERTICES,&kept_bare)==SAT_OK);
        assert(sat_character_asset_compact(&asset,small.data(),(uint32_t)(small.size()*4),0,&kept)==SAT_OK);
        assert(kept+lut_bytes==kept_plain && texture_bytes && !asset.model.luts_rgb555 && !asset.model.lut_count);
        assert(kept_bare+asset.model.vertex_count*sizeof(sat_vec3_t)==kept && !bare.model.vertices);
        assert(bare.model.indices[0]==first_face &&
            bare.model.face_texture_indices[bare.model.face_count-1u]==last_selector);
        assert(std::memcmp(&bare.skeleton.joints[0],&first_joint,sizeof(first_joint))==0);
        assert(clip_name==sat_character_clip_name(&bare,1) && std::strcmp(sat_character_joint_name(&bare,0),"root")==0);
        assert(sat_skin_validate(&bare.skin)==SAT_OK);
        assert(std::memcmp(bare.clips[0].frames,asset.clips[0].frames,
            bare.clips[0].frame_count*bare.skeleton.joint_count*sizeof(sat_skeleton_pose_joint_t))==0);
        assert(std::memcmp(&asset.model.vertices[0],&first_vertex,sizeof(first_vertex))==0);
        assert(clip_name==sat_character_clip_name(&asset,1));
        assert(std::strcmp(sat_character_joint_name(&asset,0),"root")==0);
        assert(sat_skin_validate(&asset.skin)==SAT_OK);
        std::memcpy(lut.data(),payload.data(),payload.size());
        write32(lut,SAT_CHAR_PALETTES,SAT_CHARACTER_MAX_LUT_BLOCKS+1u);
        assert(sat_character_asset_decode(lut.data(),(uint32_t)payload.size(),clips[0].data(),ClipCapacity,
            textures[0].data(),TextureCapacity,&asset)==SAT_ERR_INVALID_ARG);
    }
    std::puts("test_character_asset: two independent >64 KiB loads, random clips, release, LUT4 and malformed archives passed");
}
