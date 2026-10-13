#include "saturn/character3d.h"
#include "saturn/asset.h"
#include <vector>
#include <fstream>
#include <iterator>
#include <cassert>
#include <cstring>
#include <cstdio>

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
    std::puts("test_character_asset: two independent >64 KiB loads, random clips, release and malformed archives passed");
}
