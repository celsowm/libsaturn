#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "src/core/runtime/logic.hpp"
#include "src/hal/vdp1/vdp1.hpp"
#include "src/hal/scu/scu.hpp"

#include "saturn/vdp1.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); std::exit(1); } } while (0)

int main() {
    using namespace saturn::hal::vdp1;

    // Compare every command word against the existing writer across texture
    // formats, effects and signed corners; poisoned storage catches omissions.
    uint32_t random=0x534154u;
    for(unsigned trial=0;trial<2048;++trial) {
        auto next=[&]() { random=random*1664525u+1013904223u;return random; };
        sat_vdp1_texture_t texture{};
        texture.valid=1;texture.width=static_cast<uint16_t>((next()%63u+1u)*8u);
        texture.height=static_cast<uint16_t>(next()%255u+1u);
        texture.srca=static_cast<uint16_t>(next());texture.palette=static_cast<uint16_t>(next());
        texture.format=(trial&1)?SAT_VDP1_TEXTURE_INDEXED8:SAT_VDP1_TEXTURE_LUT4;
        const uint16_t flags=static_cast<uint16_t>(
            ((trial&2)?SAT_SPRITE_FLAG_MESH:0) |
            ((trial&4)?SAT_SPRITE_FLAG_FLIP_H:0) |
            ((trial&8)?SAT_SPRITE_FLAG_FLIP_V:0) |
            ((trial&16)?SAT_SPRITE_FLAG_USER_CLIP:0));
        sat_quad2_t quad{};DistortedSpriteRequest reference{};
        for(unsigned c=0;c<4;++c) {
            reference.x[c]=quad.x[c]=static_cast<int16_t>(next()%2048u-1024);
            reference.y[c]=quad.y[c]=static_cast<int16_t>(next()%2048u-1024);
        }
        reference.width=texture.width;reference.height=texture.height;
        reference.srca=texture.srca;reference.palette=texture.palette;
        reference.format=texture.format;reference.flags=flags;
        reference.user_clip=(flags&SAT_SPRITE_FLAG_USER_CLIP)!=0;
        Command paired[5];std::memset(paired,0xa5,sizeof(paired));
        begin_frame(paired,5);
        OK(push_distorted_sprite(reference)==SAT_OK);
        OK(push_projected_sprite(quad,texture,flags)==SAT_OK);
        OK(std::memcmp(&paired[2],&paired[3],sizeof(Command))==0);
        const Command expected=paired[2];
        uint16_t prefix[6];std::memcpy(prefix,&expected,sizeof(prefix));
        Command before[5];std::memcpy(before,paired,sizeof(paired));
        OK(push_projected_sprite(quad,texture,flags)==SAT_ERR_CAPACITY);
        OK(std::memcmp(before,paired,sizeof(paired))==0);
        begin_frame(paired,5);OK(reserve_overlay_commands(2)==SAT_OK);
        OK(push_projected_sprite(quad,texture,flags)==SAT_ERR_CAPACITY);
        OK(begin_overlay_pass()==SAT_OK);
        texture.valid=0;OK(push_projected_sprite(quad,texture,flags)==SAT_ERR_INVALID_ARG);
        texture.valid=1;texture.width=7;
        OK(push_projected_sprite(quad,texture,flags)==SAT_ERR_INVALID_ARG);
        texture.width=8;texture.height=0;
        OK(push_projected_sprite(quad,texture,flags)==SAT_ERR_INVALID_ARG);
        texture.height=8;
        OK(push_projected_sprite(quad,texture,SAT_SPRITE_FLAG_HALF_TRANSPARENT)==SAT_ERR_UNSUPPORTED);
        uint16_t used,capacity,overlay;bool overlay_pass;
        command_stats(used,capacity,overlay,overlay_pass);OK(used==2);
        OK(push_projected_sprite(quad,texture,flags)==SAT_OK);
        // Prepared prefix uses the same coordinate/footer words, END budget,
        // overlay admission and invalid-descriptor occupancy rules.
        texture.width=reference.width;texture.height=reference.height;
        begin_frame(paired,5);
        OK(push_projected_program(quad,prefix,texture)==SAT_OK);
        OK(std::memcmp(&expected,&paired[2],sizeof(Command))==0);
        OK(push_projected_program(quad,prefix,texture)==SAT_OK);
        std::memcpy(before,paired,sizeof(paired));
        OK(push_projected_program(quad,prefix,texture)==SAT_ERR_CAPACITY);
        OK(std::memcmp(before,paired,sizeof(paired))==0);
        begin_frame(paired,5);OK(reserve_overlay_commands(2)==SAT_OK);
        OK(push_projected_program(quad,prefix,texture)==SAT_ERR_CAPACITY);
        OK(begin_overlay_pass()==SAT_OK);
        texture.valid=0;
        OK(push_projected_program(quad,prefix,texture)==SAT_ERR_INVALID_ARG);
        texture.valid=1;
        OK(push_projected_program(quad,nullptr,texture)==SAT_ERR_INVALID_ARG);
        command_stats(used,capacity,overlay,overlay_pass);OK(used==2);
        OK(push_projected_program(quad,prefix,texture)==SAT_OK);
    }

    Command commands[12]{};
    begin_frame(commands, 12u);

    UserClipRequest clip{10u, 20u, 100u, 80u};
    OK(push_user_clip(clip) == SAT_OK);
    OK(commands[2].ctrl == 0x0008u);
    OK(commands[2].xa == 10 && commands[2].ya == 20);
    OK(commands[2].xc == 100 && commands[2].yc == 80);

    SpriteRequest sprite{};
    sprite.width = 8u;
    sprite.height = 8u;
    sprite.user_clip = true;
    OK(push_sprite(sprite) == SAT_OK);
    OK((commands[3].pmod & 0x0400u) != 0u);
    OK((commands[3].pmod & 0x0200u) == 0u);

    SpriteRequest mesh_sprite = sprite;
    mesh_sprite.flags = SAT_SPRITE_FLAG_MESH;
    OK(push_sprite(mesh_sprite) == SAT_OK);
    OK((commands[4].pmod & 0x0100u) != 0u);

    SpriteRequest unsupported_sprite = sprite;
    unsupported_sprite.flags = SAT_SPRITE_FLAG_HALF_TRANSPARENT;
    OK(push_sprite(unsupported_sprite) == SAT_ERR_UNSUPPORTED);
    PolygonRequest polygon{};
    polygon.color = 0x801Fu;
    polygon.flags = SAT_SPRITE_FLAG_HALF_TRANSPARENT | SAT_SPRITE_FLAG_MESH;
    OK(push_polygon(polygon) == SAT_OK);
    OK((commands[5].pmod & 0x0107u) == 0x0103u);
    polygon.color = 0x001Fu;
    OK(push_polygon(polygon) == SAT_ERR_UNSUPPORTED);
    polygon.color = 0x801Fu;
    polygon.flags = SAT_SPRITE_FLAG_HALF_LUMINANCE;
    const uint16_t corners[4] = {0x4210u,0x4210u,0x4210u,0x4210u};
    OK(push_polygon_gouraud(polygon, corners) == SAT_OK);
    OK((commands[6].pmod & 0x0007u) == 0x0006u);

    UserClipRequest invalid_order{20u, 20u, 10u, 30u};
    OK(push_user_clip(invalid_order) == SAT_ERR_INVALID_ARG);
    UserClipRequest outside{0u, 0u, 320u, 223u};
    OK(push_user_clip(outside) == SAT_ERR_INVALID_ARG);

    Command small[3]{};
    begin_frame(small, 3u);
    OK(push_user_clip(clip) == SAT_ERR_CAPACITY);

    /* sat_vdp1_set_user_clip: the rectangle is cut to the screen, and one
     * entirely off it is refused rather than sent as an inverted box. */
    {
        using namespace saturn::core;
        Vdp1UserClipRect r{};
        OK(clamp_user_clip(-8, -3, 40, 50, 320u, 224u, &r));
        OK(r.x0 == 0u && r.y0 == 0u && r.x1 == 40u && r.y1 == 50u);
        OK(clamp_user_clip(300, 200, 400, 300, 320u, 224u, &r));
        OK(r.x0 == 300u && r.y0 == 200u && r.x1 == 319u && r.y1 == 223u);
        OK(!clamp_user_clip(-20, 10, -1, 30, 320u, 224u, &r));
        OK(!clamp_user_clip(10, 230, 30, 240, 320u, 224u, &r));
        OK(!clamp_user_clip(30, 10, 29, 20, 320u, 224u, &r));
        /* SAT_SPRITE_FLAG_USER_CLIP is a sprite flag the PMOD composer leaves
         * alone; the request's user_clip turns it into Clip=1, Cmod=0. */
        OK(validate_indexed8_sprite_effects(SAT_SPRITE_FLAG_USER_CLIP | SAT_SPRITE_FLAG_FLIP_H) == SAT_OK);
        OK(validate_polygon_effects(0x801Fu, SAT_SPRITE_FLAG_USER_CLIP) == SAT_ERR_INVALID_ARG);
        OK(compose_sprite_pmod(SAT_SPRITE_FLAG_USER_CLIP) == compose_sprite_pmod(0u));
        Command flagged[8]{};
        begin_frame(flagged, 8u);
        DistortedSpriteRequest quad{};
        quad.width = 16u;
        quad.height = 16u;
        quad.flags = SAT_SPRITE_FLAG_USER_CLIP;
        quad.user_clip = true;
        OK(push_distorted_sprite(quad) == SAT_OK);
        OK((flagged[2].pmod & 0x0600u) == 0x0400u);
    }

    /* The world can never consume the two protected overlay slots or the
     * final END slot, regardless of which primitive exhausted its budget. */
    Command reserved[8]{};
    begin_frame(reserved,8u); /* two setup commands already installed */
    OK(reserve_overlay_commands(2u)==SAT_OK);
    OK(push_sprite(sprite)==SAT_OK);
    OK(push_polygon(polygon)==SAT_OK);
    OK(push_user_clip(clip)==SAT_OK);
    OK(push_sprite(sprite)==SAT_ERR_CAPACITY);
    OK(push_polygon(polygon)==SAT_ERR_CAPACITY);
    OK(push_user_clip(clip)==SAT_ERR_CAPACITY);
    OK(begin_overlay_pass()==SAT_OK);
    OK(reserve_overlay_commands(0u)==SAT_ERR_UNSUPPORTED);
    OK(push_sprite(sprite)==SAT_OK);
    OK(push_polygon(polygon)==SAT_OK);
    OK(push_sprite(sprite)==SAT_ERR_CAPACITY);
    /* New frame clears the reservation and restores normal command budget. */
    begin_frame(reserved,8u);
    for(int i=0;i<5;++i) OK(push_sprite(sprite)==SAT_OK);
    OK(push_sprite(sprite)==SAT_ERR_CAPACITY);
    begin_frame(reserved,8u);
    OK(reserve_overlay_commands(6u)==SAT_ERR_CAPACITY);
    OK(push_sprite(sprite)==SAT_OK);

    /* RGB CLUT sprites preserve source and CLUT while selecting Gouraud. */
    Command lut_commands[8]{};
    begin_frame(lut_commands,8u);
    DistortedSpriteRequest lut_quad{};
    lut_quad.width=16;lut_quad.height=16;lut_quad.srca=0x3456;
    lut_quad.palette=0x2345;lut_quad.format=SAT_VDP1_TEXTURE_LUT4;
    OK(push_distorted_sprite_gouraud(lut_quad,corners)==SAT_OK);
    OK((lut_commands[2].pmod&0x003fu)==0x000cu);
    OK(lut_commands[2].colr==lut_quad.palette);
    OK(lut_commands[2].srca==lut_quad.srca);
    OK(lut_commands[2].grda==65536u/8u);
    lut_quad.format=SAT_VDP1_TEXTURE_INDEXED8;
    OK(push_distorted_sprite_gouraud(lut_quad,corners)==SAT_ERR_UNSUPPORTED);
    lut_quad.format=SAT_VDP1_TEXTURE_LUT4;
    OK(push_distorted_sprite_gouraud(lut_quad,nullptr)==SAT_ERR_INVALID_ARG);
    /* Failed commands must release their staged table. */
    lut_quad.width=0;
    for(unsigned i=0;i<2200;++i)
        OK(push_distorted_sprite_gouraud(lut_quad,corners)!=SAT_ERR_CAPACITY);
    lut_quad.width=16;
    OK(push_distorted_sprite_gouraud(lut_quad,corners)==SAT_OK);
    OK(lut_commands[3].grda==65536u/8u+1u);

    std::puts("vdp1 clip: OK");
    return 0;
}
