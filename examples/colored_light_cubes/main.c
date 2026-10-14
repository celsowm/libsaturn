#include "saturn/saturn.h"
#include "saturn/app.h"
#include "saturn/font.h"
#include "example_util.h"
#include "../killer_clown_demo/lighting.h"

enum { WIDTH=320, HEIGHT=224, SIDE=64, FACE_CAPACITY=16,
       PERIOD=360, FLOOR_NAMES=0, FLOOR_CHARS=0x40000, FLOOR_PALETTE=16,
       POOL_BANK=5, OVERLAY_COMMANDS=128 };
static sat_scene_t scene;
static sat_scene3d_face_t faces[FACE_CAPACITY];
static uint32_t keys[FACE_CAPACITY];
static uint16_t order[FACE_CAPACITY],names[64*64],tiles[32];
static uint8_t checker[SIDE*SIDE/2],radial[SIDE*SIDE];
static sat_vdp1_texture_t cube_texture,pool_texture;
static sat_ascii_font_t font;
volatile struct { uint32_t frames, paused, lights, pool, phase, errors; } g_light_cubes;
static const int8_t corners[8][3]={{-1,0,-1},{1,0,-1},{1,2,-1},{-1,2,-1},
    {-1,0,1},{1,0,1},{1,2,1},{-1,2,1}};
static const uint8_t topology[6][4]={{4,5,6,7},{1,0,3,2},{5,1,2,6},
    {0,4,7,3},{7,6,2,3},{0,1,5,4}};
static const int8_t normals[6][3]={{0,0,1},{0,0,-1},{1,0,0},
    {-1,0,0},{0,1,0},{0,-1,0}};
static void check(sat_result_t result) {
    if(result!=SAT_OK)++g_light_cubes.errors;
    sat_example_must(result);
}
static void label(const char* text,int x,int y) {
    check(sat_ascii_font_draw_text_screen_indexed8(&font,text,x,y,8,0,0));
}
static void initialize(void) {
    /* Font bank zero must be uploaded before the NBG0 subpalette. */
    check(sat_ascii_font_init_8x8_indexed8(&font,SAT_COLOR_WHITE,0,0));
    uint16_t lut[16]={0};lut[1]=SAT_RGB555(12,12,12);lut[2]=SAT_RGB555(18,18,18);
    for(unsigned y=0;y<SIDE;++y)for(unsigned x=0;x<SIDE;x+=2)
        checker[(y*SIDE+x)/2]=((x/8+y/8)&1)?0x11:0x22;
    uint16_t handle;check(sat_vdp1_upload_lut(lut,&handle));
    check(sat_tex_upload_lut4_pixels(&cube_texture,checker,SIDE,SIDE,handle));
    uint16_t palette[256]={0};
    for(unsigned i=1;i<32;++i)palette[i]=SAT_RGB555(i/2,i/8,i/16);
    for(unsigned y=0;y<SIDE;++y)for(unsigned x=0;x<SIDE;++x) {
        int dx=(int)x-SIDE/2,dy=(int)y-SIDE/2;
        unsigned radius=SIDE*SIDE/4,square=(unsigned)(dx*dx+dy*dy);
        radial[y*SIDE+x]=square>=radius?0:(uint8_t)(1+30*(radius-square)/radius);
    }
    check(sat_tex_upload_indexed8(&pool_texture,radial,SIDE,SIDE,palette,POOL_BANK));
    uint16_t floor_palette[16]={0};floor_palette[1]=SAT_RGB555(5,6,7);floor_palette[2]=SAT_RGB555(8,9,10);
    check(sat_vdp2_palette_upload(floor_palette,16,FLOOR_PALETTE));
    for(unsigned i=0;i<16;++i){tiles[i]=0x1111;tiles[16+i]=0x2222;}
    for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x)
        names[y*64+x]=0x1000|((x/2+y/2)&1);
    check(sat_vdp2_vram_write_words(FLOOR_NAMES/2,names,64*64));
    check(sat_vdp2_vram_write_words(FLOOR_CHARS/2,tiles,32));
    sat_vdp2_layer_config_t config;sat_vdp2_layer_config_default(SAT_VDP2_NBG0,&config);
    config.char_base_address=FLOOR_CHARS;config.char_bank_mask=SAT_VDP2_BANK_B0;
    config.priority=3;config.transparent=0;
    for(unsigned i=0;i<4;++i)config.plane_address[i]=FLOOR_NAMES;
    check(sat_vdp2_layer_configure(&config));
    check(sat_vdp2_sprite_color_calc_configure_alpha(7));
    check(sat_scene_init(&scene,faces,keys,order,FACE_CAPACITY));
    g_light_cubes.lights=g_light_cubes.pool=1;
}
static void draw_cube(unsigned actor,const sat_camera3d_t* camera,const clown_spotlight_t lights[2]) {
    sat_fx16_t center=(actor?1:-1)*SAT_FX16_ONE*6/5;
    for(unsigned face=0;face<6;++face) {
        sat_vec3_t normal={normals[face][0]*SAT_FX16_ONE,normals[face][1]*SAT_FX16_ONE,normals[face][2]*SAT_FX16_ONE};
        sat_vec3_t toward={camera->eye.x-center-normal.x/2,camera->eye.y-SAT_FX16_ONE/2-normal.y/2,camera->eye.z-normal.z/2};
        if(sat_vec3_dot(&normal,&toward)<=0)continue;
        sat_quad3_t quad;uint16_t gouraud[4];
        for(unsigned i=0;i<4;++i) {
            const int8_t* v=corners[topology[face][i]];
            quad.v[i]=(sat_vec3_t){center+v[0]*SAT_FX16_ONE/2,v[1]*SAT_FX16_ONE/2,v[2]*SAT_FX16_ONE/2};
            unsigned red=clown_spotlight_diffuse(&lights[0],quad.v[i],normal);
            unsigned blue=clown_spotlight_diffuse(&lights[1],quad.v[i],normal);
            gouraud[i]=sat_gouraud_rgb((int)(red*15/256), (int)((red+blue)*3/256), (int)(blue*15/256));
        }
        sat_scene3d_material_t material={0};material.kind=SAT_SCENE3D_INDEXED_TEXTURED;
        material.texture=&cube_texture;material.color_calc_slot=SAT_INDEXED_SOLID_OPAQUE;
        if(actor && g_light_cubes.lights)material.vertex_gouraud=gouraud;
        check(sat_scene_submit_quad(&scene,&quad,&material,1));
    }
}
int main(void) {
    check(sat_app_init_default());check(sat_vdp1_set_frame_mode(SAT_VDP1_FRAME_ON_SUBMIT));initialize();
    sat_camera3d_t camera; sat_vec3_t eye={0,3*SAT_FX16_ONE,6*SAT_FX16_ONE};
    sat_vec3_t target={0,SAT_FX16_ONE/2,0},up={0,SAT_FX16_ONE,0};
    check(sat_camera3d_init(&camera,&eye,&target,&up,50*SAT_FX16_ONE,
        WIDTH*SAT_FX16_ONE/HEIGHT,SAT_FX16_ONE/8,20*SAT_FX16_ONE));
    while(1) {
        sat_pad_state_t pad={0};check(sat_wait_vblank());
        check(sat_vdp1_set_erase_transparent());check(sat_begin_frame());
        check(sat_pad_poll(&pad));
        if(pad.pressed&SAT_PAD_A)g_light_cubes.paused^=1;
        if(pad.pressed&SAT_PAD_B)g_light_cubes.lights^=1;
        if(pad.pressed&SAT_PAD_C)g_light_cubes.pool^=1;
        if(!g_light_cubes.paused)g_light_cubes.phase=(g_light_cubes.phase+1)%PERIOD;
        sat_fx16_t angle=g_light_cubes.phase*SAT_FX16_ONE;
        sat_vec3_t aim={SAT_FX16_ONE*6/5,SAT_FX16_ONE/2,0};clown_spotlight_t lights[2];
        for(unsigned i=0;i<2;++i) {
            sat_vec3_t source={aim.x+(i?-1:1)*SAT_FX16_ONE+sat_cos_deg(angle)*2/5,
                SAT_FX16_ONE*5/2,2*SAT_FX16_ONE+sat_sin_deg(angle)*2/5};
            clown_spotlight_aim(&lights[i],source,aim);
        }
        check(sat_scene_begin(&scene,&camera,SAT_FX16_ONE/8,WIDTH,HEIGHT,OVERLAY_COMMANDS));
        if(g_light_cubes.pool) {
            sat_quad3_t pool;
            for(unsigned i=0;i<4;++i)clown_spotlight_ground(&lights[0],(45+i*90)*SAT_FX16_ONE,&pool.v[i]);
            sat_scene3d_material_t material={0};material.kind=SAT_SCENE3D_INDEXED_TEXTURED;
            material.texture=&pool_texture;material.color_calc_slot=SAT_VDP2_SPRITE_ADD;
            check(sat_scene_submit_quad(&scene,&pool,&material,0));
        }
        draw_cube(0,&camera,lights);draw_cube(1,&camera,lights);check(sat_scene_flush(&scene));
        label("COLORED LIGHT / TWO CUBES",64,16);
        label("REFERENCE",40,48);label("GOURAUD",208,48);
        label("A PAUSE  B CUBE LIGHT  C FLOOR",40,200);
        check(sat_app_frame_end());++g_light_cubes.frames;
    }
}
