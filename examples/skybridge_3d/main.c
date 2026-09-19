/* Skybridge 3D: a playable stock-Saturn platformer, VDP1 world + VDP2 sea/sky. */
#include <stdint.h>
#include "saturn/saturn.h"
#include "saturn/asset.h"
#include "saturn/example_util.h"
#include "examples/vdp2_rbg0_ground/rbg0_math.h"
#include "game.h"

#define W 320u
#define H 224u
#define HORIZON 96u
#define SEA_WORD 0x00000u
#define ROT_WORD 0x10000u
#define COEF_WORD 0x12000u
#define SKY_W 512u
#define SKY_H 128u
#define VIEW_LIMIT SB_F(116)
#define SOUNDS 6u
#define SOUND_LEN 2048u
#define MUSIC_LEN 32768u
#define RENDER_COUNT (SB_PLATFORM_COUNT + 1u)

typedef struct sb_render_item {
    int8_t id;    /* -1=player; 0..9=platform and its collectible */
    int32_t depth;
} sb_render_item_t;

static sb_game_t g_game;
static sat_ascii_font_t g_font;
static sat_vdp1_texture_t g_tile_texture;
static uint8_t g_tile_pixels[16u*16u];
static uint8_t g_sky[SKY_W * SKY_H];
static uint16_t g_sky_colors[256], g_sea_colors[256];
static uint16_t g_map[SAT_VDP2_NBG0_MAP_CELLS];
static sat_mat4_t g_vp;
static sat_vec3_t g_eye, g_target, g_camera_anchor;
static sb_render_item_t g_items[RENDER_COUNT];
static uint8_t g_items_count;
static int16_t g_yaw;
static uint32_t g_prev_frame, g_frame;
static sat_sound_t g_sounds[SOUNDS];
static sat_voice_t g_music_voice;
static const char* const g_sfx_paths[SOUNDS-1u]={
    "skybridge/sfx/jump", "skybridge/sfx/land", "skybridge/sfx/pickup",
    "skybridge/sfx/fall", "skybridge/sfx/finish"
};
static int8_t g_audio[SOUNDS - 1u][SOUND_LEN];
static int8_t g_music[MUSIC_LEN];
static uint8_t g_audio_ready;
static uint8_t g_show_help=1u;
static const rbg0_ground_config_t g_ocean = {
    512u, 256u, 160u, HORIZON, 96u, 8u, 96u, COEF_WORD
};

/* Boot is a series of completed jobs, not a pretend timed progress bar.
 * VDP1 draws this fullscreen panel independently of whether VDP2 is ready. */
static void loading_frame(const char* stage, uint8_t percent) {
    char progress[28];
    uint16_t width=(uint16_t)((uint32_t)percent*252u/100u);
    sat_example_must(sat_wait_vblank());
    sat_example_must(sat_begin_frame());
    sat_example_must(sat_draw_rect_screen(0,0,W,H,SAT_RGB555(2,7,14)));
    sat_example_must(sat_draw_rect_screen(34,64,252u,3u,SAT_RGB555(12,26,27)));
    sat_example_must(sat_ascii_font_draw_text_screen_centered_indexed8(
        &g_font,"SKYBRIDGE 3D",160,77,8,0u,0u));
    sat_example_must(sat_ascii_font_draw_text_screen_centered_indexed8(
        &g_font,stage,160,110,8,0u,0u));
    sat_example_must(sat_draw_rect_screen(34,137,252u,9u,SAT_RGB555(6,13,17)));
    if(width>0u) sat_example_must(sat_draw_rect_screen(
        34,137,width,9u,SAT_RGB555(20,29,19)));
    sat_example_must(sat_fmt_label_u32("LOADING ",percent,progress,sizeof(progress),0));
    sat_example_must(sat_ascii_font_draw_text_screen_centered_indexed8(
        &g_font,progress,160,153,8,0u,0u));
    sat_example_must(sat_end_frame());
}
static void put_text(const char* text, int x, int y) {
    (void)sat_ascii_font_draw_text_screen_indexed8(
        &g_font, text, x, y, 8, 0u, 0u);
}
static void label(const char* title, uint32_t n, int x, int y) {
    char buf[32];
    if (sat_fmt_label_u32(title, n, buf, sizeof(buf), 0) == SAT_OK) put_text(buf,x,y);
}
static void put_quad(const sat_quad3_t* q, uint16_t c) {
    /* A whole quad crossing the near plane is intentionally rejected by this API. */
    sat_result_t st = sat_draw_world_polygon(&g_vp,q,c);
    if (st != SAT_OK && st != SAT_ERR_UNSUPPORTED) sat_example_must(st);
}
static void put_quad_lit(const sat_quad3_t* q, uint16_t c) {
    const uint16_t light[4]={
        SAT_GOURAUD_NEUTRAL, sat_gouraud_from_intensity(SB_F(7)/8),
        sat_gouraud_from_intensity(SB_F(3)/4), sat_gouraud_from_intensity(SB_F(7)/8)
    };
    sat_result_t st=sat_draw_world_polygon_gouraud(&g_vp,q,c,light);
    if (st!=SAT_OK && st!=SAT_ERR_UNSUPPORTED) sat_example_must(st);
}
static void pquad(sat_quad3_t* q, int32_t ax,int32_t ay,int32_t az,
                  int32_t bx,int32_t by,int32_t bz,
                  int32_t cx,int32_t cy,int32_t cz,
                  int32_t dx,int32_t dy,int32_t dz) {
    q->v[0]=(sat_vec3_t){ax,ay,az};
    q->v[1]=(sat_vec3_t){bx,by,bz};
    q->v[2]=(sat_vec3_t){cx,cy,cz};
    q->v[3]=(sat_vec3_t){dx,dy,dz};
}
static void quad_rect_xz(sat_quad3_t* q,int32_t lx,int32_t rx,int32_t z0,int32_t z1,int32_t y) {
    pquad(q,lx,y,z0, rx,y,z0, rx,y,z1, lx,y,z1);
}
/* Draw just the top and camera-facing sides, always with consistent thickness. */
static void box3(int32_t x,int32_t y,int32_t z,int32_t hx,int32_t hy,int32_t hz,
                 uint16_t top,uint16_t xcolor,uint16_t zcolor,uint8_t trim) {
    sat_quad3_t q;
    int32_t lx=x-hx,rx=x+hx,by=y-hy,bz=z-hz,fz=z+hz;
    if (g_eye.x>x) {
        pquad(&q,rx,y,bz,rx,y,fz,rx,by,fz,rx,by,bz);
    } else {
        pquad(&q,lx,y,fz,lx,y,bz,lx,by,bz,lx,by,fz);
    }
    put_quad(&q,xcolor);
    if (g_eye.z>z) {
        pquad(&q,rx,y,fz,lx,y,fz,lx,by,fz,rx,by,fz);
    } else {
        pquad(&q,lx,y,bz,rx,y,bz,rx,by,bz,lx,by,bz);
    }
    put_quad(&q,zcolor);
    if (g_eye.y>y) {
        quad_rect_xz(&q,lx,rx,bz,fz,y);
        put_quad_lit(&q,top);
        if (trim && hx>SB_F(4) && hz>SB_F(4)) {
            /* Indexed VDP1 distorted sprite: one tiny reusable 16x16 material. */
            quad_rect_xz(&q,lx+SB_F(2),rx-SB_F(2),bz+SB_F(2),fz-SB_F(2),y+SB_F(1)/32);
            if (trim==2u) {
                put_quad(&q,SAT_RGB555(24,20,10));
            } else {
                sat_result_t st=sat_draw_world_sprite(&g_vp,&q,&g_tile_texture,0u,0u);
                if (st!=SAT_OK && st!=SAT_ERR_UNSUPPORTED) sat_example_must(st);
            }
        }
    }
}
static uint16_t top_color(uint8_t i) {
    if (i<4u) return SAT_RGB555(24,23,16);
    if (i<7u) return SAT_RGB555(12,26,19);
    return SAT_RGB555(27,23,16);
}
static void stage_box(uint8_t i) {
    const sb_platform_t* p=&sb_stage[i];
    int32_t x=sb_platform_x(&g_game,i), z=SB_F(p->z);
    uint16_t t=top_color(i);
    if (i==7u && g_game.collapse_ticks>0u && g_game.collapse_ticks<=30u)
        t=(g_game.ticks&4u)?SAT_RGB555(31,8,5):SAT_RGB555(31,26,5);
    box3(x,SB_F(p->y),z,SB_F(p->half_x),SB_F(5),SB_F(p->half_z),
         t, i<4u ? SAT_RGB555(12,14,14):SAT_RGB555(6,16,15),
         i<4u ? SAT_RGB555(17,17,14):SAT_RGB555(8,19,18),
         (i==7u)?2u:1u);
    /* Checkpoints and finish are physically marked, not just HUD text. */
    if (i==3u || i==6u) {
        box3(x-SB_F(7),SB_F(p->y+5),z+SB_F(5),SB_F(1),SB_F(5),SB_F(1),
             SAT_RGB555(31,26,3),SAT_RGB555(23,16,3),SAT_RGB555(31,19,2),0u);
    }
    if (i==9u) {
        box3(x-SB_F(10),SB_F(p->y+9),z+SB_F(5),SB_F(1),SB_F(9),SB_F(1),
             SAT_RGB555(31,26,3),SAT_RGB555(16,12,3),SAT_RGB555(27,20,4),0u);
        box3(x+SB_F(10),SB_F(p->y+9),z+SB_F(5),SB_F(1),SB_F(9),SB_F(1),
             SAT_RGB555(31,26,3),SAT_RGB555(16,12,3),SAT_RGB555(27,20,4),0u);
        box3(x,SB_F(p->y+19),z+SB_F(5),SB_F(11),SB_F(1),SB_F(1),
             SAT_RGB555(31,26,3),SAT_RGB555(20,14,3),SAT_RGB555(27,19,4),0u);
    }
    if (i>=1u && i<=8u && !(g_game.pickups & (1u<<(i-1u)))) {
        const int32_t wave=sat_fx16_mul(sat_sin_deg(SB_F((int32_t)(g_frame*5u+i*33u)%360)),SB_F(1)/2);
        box3(x,SB_F(p->y+7)+wave,z,SB_F(2),SB_F(2),SB_F(2),
             SAT_RGB555(31,30,8),SAT_RGB555(31,13,3),SAT_RGB555(31,23,4),0u);
    }
}
static void player_box(void) {
    int32_t px=g_game.x,pz=g_game.z,feet=g_game.y;
    uint16_t top=SAT_RGB555(31,28,7),front=SAT_RGB555(31,12,4);
    int32_t bob=0;
    if (g_game.support>=0 && (sb_abs(g_game.vx)+sb_abs(g_game.vz))>SB_F(1)/2)
        bob=sat_fx16_mul(sat_sin_deg(SB_F((int32_t)(g_frame*12u)%360)),SB_F(1)/7);
    if (g_game.support>=0) {
        const sb_platform_t* p=&sb_stage[(uint8_t)g_game.support];
        sat_quad3_t shadow;
        int32_t sy=SB_F(p->y)+SB_F(1)/16;
        quad_rect_xz(&shadow,px-SB_F(3),px+SB_F(3),pz-SB_F(3),pz+SB_F(3),sy);
        (void)sat_draw_world_polygon_effects(&g_vp,&shadow,SAT_RGB555(4,6,6),SAT_SPRITE_FLAG_MESH);
    }
    box3(px,feet+SB_F(5)/2+bob,pz,SB_PLAYER_HALF,SB_F(5)/2,SB_PLAYER_HALF,
         top,SAT_RGB555(31,20,6),front,0u);
    /* Direction marker, attached to the cube's camera-facing surface. */
    {
        sat_quad3_t q;
        if (g_eye.z < pz) {
            pquad(&q,px-SB_F(1),feet+SB_F(4)+bob,pz-SB_PLAYER_HALF-SB_F(1)/32,
                  px+SB_F(1),feet+SB_F(4)+bob,pz-SB_PLAYER_HALF-SB_F(1)/32,
                  px+SB_F(1),feet+SB_F(2)+bob,pz-SB_PLAYER_HALF-SB_F(1)/32,
                  px-SB_F(1),feet+SB_F(2)+bob,pz-SB_PLAYER_HALF-SB_F(1)/32);
            put_quad(&q,SAT_RGB555(31,31,31));
        }
    }
}
static int32_t view_depth(int32_t x,int32_t z,int32_t forward_x,int32_t forward_z) {
    return sb_mul(x-g_eye.x,forward_x)+sb_mul(z-g_eye.z,forward_z);
}
static void draw_world(int32_t forward_x,int32_t forward_z) {
    uint8_t i,j;
    g_items_count=0u;
    for (i=0u;i<SB_PLATFORM_COUNT;++i) {
        int32_t px=sb_platform_x(&g_game,i),pz=SB_F(sb_stage[i].z);
        int32_t depth=view_depth(px,pz,forward_x,forward_z);
        if (!sb_platform_active(&g_game,i) || depth < -SB_F(9) || depth>VIEW_LIMIT ||
            sb_abs(px-g_game.x)>SB_F(100) || sb_abs(pz-g_game.z)>SB_F(110)) continue;
        g_items[g_items_count++]=(sb_render_item_t){(int8_t)i,depth};
    }
    g_items[g_items_count++]=(sb_render_item_t){-1,
        view_depth(g_game.x,g_game.z,forward_x,forward_z)};
    /* One global far-to-near object order; no illusion of a depth buffer. */
    for (i=1u;i<g_items_count;++i) {
        sb_render_item_t cur=g_items[i];
        j=i;
        while(j>0u && g_items[j-1u].depth<cur.depth) {
            g_items[j]=g_items[j-1u]; --j;
        }
        g_items[j]=cur;
    }
    for (i=0u;i<g_items_count;++i) {
        if (g_items[i].id==-1) player_box();
        else stage_box((uint8_t)g_items[i].id);
    }
}
static void init_tile_texture(void) {
    uint16_t palette[256];
    uint16_t x,y;
    for (x=0u;x<256u;++x) palette[x]=SAT_BGR555(0,0,0);
    palette[1]=SAT_BGR555(9,23,18);
    palette[2]=SAT_BGR555(16,28,22);
    palette[3]=SAT_BGR555(26,30,27);
    for (y=0u;y<16u;++y) for (x=0u;x<16u;++x) {
        uint8_t idx=(uint8_t)((x%4u==0u || y%4u==0u)?2u:1u);
        if ((x+y)%11u==0u) idx=3u;
        g_tile_pixels[y*16u+x]=idx;
    }
    sat_example_must(sat_tex_upload_indexed8(
        &g_tile_texture,g_tile_pixels,16u,16u,palette,3u));
}
static void init_sky(void) {
    uint32_t y,x;
    for (y=0;y<256u;++y) {
        uint32_t t=y<128u?y:127u;
        g_sky_colors[y]=SAT_BGR555((t*17u)/127u+2u,
            (t*19u)/127u+8u,(t*8u)/127u+21u);
    }
    g_sky_colors[160]=SAT_BGR555(27,28,29);
    g_sky_colors[161]=SAT_BGR555(23,27,29);
    g_sky_colors[162]=SAT_BGR555(13,19,24);
    g_sky_colors[163]=SAT_BGR555(10,16,20);
    for (y=0u;y<SKY_H;++y) for (x=0u;x<SKY_W;++x) {
        uint32_t at=y*SKY_W+x;
        uint32_t wave=((x*13u + (x>>3u)*19u)&63u);
        uint32_t cloud=(x/57u + (x/19u)%3u)*7u;
        uint8_t index=(uint8_t)(y+1u);
        if (y>42u && y<70u && ((x+cloud)&127u)>24u &&
            ((y-49u)*3u + (x&15u))%71u < 13u)
            index=(uint8_t)(160u+((x>>4u)&1u));
        /* Low mountains, behind the RBG0 sea at the horizon. */
        if (y>112u+(wave>>3u) && y<126u) index=(uint8_t)(162u+((x>>5u)&1u));
        g_sky[at]=index;
    }
}
static void init_sea(void) {
    volatile uint16_t* vram=(volatile uint16_t*)0x25E00000u;
    uint32_t x,y,at=SEA_WORD;
    for (x=0u;x<256u;++x) {
        uint32_t t=x&31u;
        g_sea_colors[x]=SAT_BGR555(2u+t/5u,8u+t/2u,17u+t/3u);
    }
    for (y=0u;y<256u;++y) for (x=0u;x<512u;x+=2u) {
        uint32_t a=((x*5u)^(y*13u)^(x>>3u))&31u;
        uint32_t b=(((x+1u)*5u)^(y*13u)^((x+1u)>>3u))&31u;
        uint8_t va=(uint8_t)(1u+(a>>1u)+(((y+(x>>4u))&31u)<3u?9u:0u));
        uint8_t vb=(uint8_t)(1u+(b>>1u)+(((y+((x+1u)>>4u))&31u)<3u?9u:0u));
        vram[at++]=(uint16_t)(((uint16_t)va<<8u)|vb);
        if(x==510u && (y&15u)==15u)
            loading_frame("GENERATING OCEAN",(uint8_t)(15u+((y+1u)*60u/256u)));
    }
}
static void update_rotation(int32_t fx,int32_t fz) {
    uint16_t p[48];
    /* Keep the sea under a fixed 96px horizon, rotate/scroll sample plane. */
    rbg0_ground_build_params(&g_ocean, (g_game.x>>16)+(int32_t)(g_frame>>2u),
                              (g_game.z>>16)+(int32_t)(g_frame>>3u),p);
    p[15]=(uint16_t)((uint32_t)fz&0xFFFFu);
    p[17]=(uint16_t)((uint32_t)fx&0xFFFFu);
    p[21]=(uint16_t)((uint32_t)(-fx)&0xFFFFu);
    p[23]=(uint16_t)((uint32_t)fz&0xFFFFu);
    /* The parameter table uses signed 16.16 A/B/D/E, including high words. */
    p[14]=(uint16_t)(fz>>16);
    p[16]=(uint16_t)(fx>>16);
    p[20]=(uint16_t)((-fx)>>16);
    p[22]=(uint16_t)(fz>>16);
    sat_example_must(sat_vdp2_vram_write_words(ROT_WORD,p,48u));
}
static void init_background(void) {
    uint16_t coef[H*2u],p[48];
    const sat_vdp2_nbg0_config_t sky = {
        SAT_VDP2_CHAR_SIZE_1X1,SAT_VDP2_COLOR_MODE_256,0x3Bu,0u,0u
    };
    const sat_vdp2_rbg0_mode7_config_t sea = {
        SAT_VDP2_RBG0_BITMAP_512x256,SAT_VDP2_COLOR_MODE_256,
        SEA_WORD,ROT_WORD,SAT_COLOR_BLACK,5u,7u
    };
    uint32_t y;
    for(y=0u;y<H;++y) rbg0_ground_encode_coefficient(&g_ocean,y,
                      &coef[y*2u],&coef[y*2u+1u]);
    rbg0_ground_build_params(&g_ocean,0,0,p);
    sat_example_must(sat_vdp2_palette_upload(g_sea_colors,256u,0u));
    sat_example_must(sat_vdp2_palette_upload(g_sky_colors,256u,256u));
    sat_example_must(sat_vdp2_nbg0_init(&sky));
    sat_example_must(sat_vdp2_nbg0_upload_indexed8(g_sky,SKY_W,SKY_H,1u,g_map));
    sat_example_must(sat_vdp2_vram_write_words(COEF_WORD,coef,H*2u));
    sat_example_must(sat_vdp2_vram_write_words(ROT_WORD,p,48u));
    sat_example_must(sat_vdp2_rbg0_mode7_init(&sea));
    sat_example_must(sat_vdp2_nbg0_set_priority(2u));
    sat_example_must(sat_vdp2_rbg0_set_priority(5u));
    sat_example_must(sat_vdp2_sprite_set_priority(7u));
    loading_frame("COMPOSING VDP2",82u);
}
static void init_audio(void) {
    uint32_t i;
    uint8_t j;
    const uint8_t tone_step[SOUNDS-1u]={3u,5u,7u,2u,9u};
    sat_sound_play_params_t music_params={76u,0,1u,0u,SB_F(1)};
    sat_example_must(sat_audio_init());
    sat_example_must(sat_audio_set_master_volume(160u));
    for(j=0u;j<SOUNDS-1u;++j) {
        for(i=0u;i<SOUND_LEN;++i) {
            int32_t wave=(int32_t)((i*tone_step[j])&63u)-32;
            int32_t env=(int32_t)((SOUND_LEN-i)*55u/SOUND_LEN);
            if (j==3u) wave=(int32_t)(((i*73u)^(i>>2u))&63u)-32;
            g_audio[j][i]=(int8_t)(wave*env/32);
        }
        {
            sat_asset_desc_t asset={0};
            sat_asset_t asset_handle={0};
            asset.logical_path=g_sfx_paths[j];
            asset.kind=SAT_ASSET_SOUND;
            asset.data=g_audio[j];
            asset.size=SOUND_LEN;
            asset.sample_rate=11025u;
            asset.sample_count=SOUND_LEN;
            asset.channels=1u;
            asset.format=SAT_AUDIO_PCM_S8;
            sat_example_must(sat_asset_register(&asset,&asset_handle));
            sat_example_must(sat_sound_load(g_sfx_paths[j],&g_sounds[j]));
        }
        loading_frame("REGISTERING AUDIO",(uint8_t)(82u+(j+1u)*2u));
    }
    {
        static const uint8_t periods[8]={50u,45u,40u,38u,34u,38u,40u,45u};
        for (i=0u;i<MUSIC_LEN;++i) {
            uint32_t note=i/4096u, local=i%4096u;
            uint32_t period=periods[note];
            int32_t phase=(int32_t)((local%period)*128u/period);
            int32_t wave=(phase<64 ? phase : 128-phase)-32;
            int32_t envelope=(int32_t)(local<160u ? local :
                (local>3935u ? 4095u-local : 160u));
            int32_t bass=(int32_t)((i%128u)/2u);
            bass=(bass<32 ? bass : 64-bass)-16;
            /* 8-note, three-second arpeggio. Fade notes to zero at boundaries. */
            g_music[i]=(int8_t)((wave*envelope)/320+bass/4);
            if ((i&4095u)==4095u)
                loading_frame("GENERATING MUSIC",(uint8_t)(92u+(i+1u)*6u/MUSIC_LEN));
        }
    }
    {
        sat_sound_desc_t desc={0};
        desc.samples=g_music;desc.sample_count=MUSIC_LEN;
        desc.sample_rate=11025u;desc.format=SAT_AUDIO_PCM_S8;
        desc.loop=1u;
        sat_example_must(sat_sound_create(&g_sounds[5u],&desc));
        sat_example_must(sat_sound_play(g_sounds[5u],&music_params,&g_music_voice));
    }
    g_audio_ready=1u;
    loading_frame("READY",100u);
}
static void sound_event(uint16_t event) {
    uint8_t id;
    sat_sound_play_params_t p={96u,0,8u,0u,SB_F(1)};
    if (!g_audio_ready || !event) return;
    if (event&SB_EVENT_WIN) id=4u;
    else if (event&SB_EVENT_FALL) id=3u;
    else if (event&SB_EVENT_CHECKPOINT) id=2u;
    else if (event&SB_EVENT_PICKUP) id=2u;
    else if (event&SB_EVENT_JUMP) id=0u;
    else if (event&SB_EVENT_LAND) id=1u;
    else if (event&SB_EVENT_WARNING) id=1u;
    else return;
    (void)sat_sound_play(g_sounds[id],&p,0);
}
static void hud(void) {
    uint8_t i,count=0u;
    for(i=0u;i<SB_PICKUP_COUNT;++i) if(g_game.pickups&(1u<<i)) ++count;
    (void)sat_draw_rect_screen(0,0,W,16u,SAT_RGB555(3,8,15));
    put_text("SKYBRIDGE 3D",7,4);
    label("GEMS ",count,144,4);
    label("TIME ",g_game.ticks/60u,224,4);
    if (g_game.finished) {
        (void)sat_draw_rect_screen(46,82,228u,53u,SAT_RGB555(2,13,16));
        put_text("COURSE COMPLETE!",80,88);
        put_text("START: PLAY AGAIN",82,106);
    } else if (g_game.paused) {
        put_text("PAUSED - START RESUMES",64,92);
    } else if (g_game.support==9 && g_game.pickups!=0xFFu) {
        put_text("COLLECT ALL 8 GEMS",65,35);
    } else if(g_show_help && g_game.ticks<480u) {
        put_text("D-PAD MOVE  A JUMP",8,204);
        put_text("B/C CAMERA  START PAUSE",8,215);
    }
}
int main(void) {
    const sat_video_config_t video={W,H,1u,0u};
    const sat_vec3_t up={0,SB_F(1),0};
    sat_pad_state_t pad={0};
    sat_vdp2_scroll_t sky_scroll={0u,0u,31u,0u};
    sat_example_must(sat_init(&video));
    sb_init(&g_game);
    g_camera_anchor=(sat_vec3_t){g_game.x,g_game.y,g_game.z};
    /* Load the first drawable font before expensive procedural generation. */
    sat_example_must(sat_ascii_font_init_8x8_indexed8(
        &g_font,SAT_COLOR_WHITE,SAT_COLOR_BLACK,2u));
    sat_example_must(sat_vdp1_set_erase_transparent());
    loading_frame("INITIALIZING WORLD",5u);
    init_tile_texture();
    loading_frame("BUILDING SKY",10u);
    init_sky();
    loading_frame("BUILDING SEA",15u);
    init_sea();
    init_background();
    init_audio();
    g_prev_frame=sat_frame_count();
    for(;;) {
        uint32_t now,steps;
        uint16_t pressed,events=0u;
        int32_t fx,fz,rx,rz;
        sat_example_must(sat_wait_vblank());
        sat_example_must(sat_pad_poll(&pad));
        now=sat_frame_count();
        steps=now-g_prev_frame;
        g_prev_frame=now;
        if(steps>3u) steps=3u; /* Drop excess catch-up, preserve responsive input. */
        if (steps==0u) steps=1u;
        if (pad.pressed&SAT_PAD_START) {
            if(g_game.finished) {sb_init(&g_game);g_yaw=0;
                g_camera_anchor=(sat_vec3_t){g_game.x,g_game.y,g_game.z};}
            else g_game.paused=(uint8_t)!g_game.paused;
        }
        if(pad.pressed&SAT_PAD_B) g_yaw-=15;
        if(pad.pressed&SAT_PAD_C) g_yaw+=15;
        if(g_yaw>=360)g_yaw-=360;
        if(g_yaw<0)g_yaw+=360;
        fx=sat_sin_deg(SB_F(g_yaw));
        fz=sat_cos_deg(SB_F(g_yaw));
        rx=fz;rz=-fx;
        pressed=0u;
        if (pad.pressed&SAT_PAD_A) pressed|=SB_JUMP;
        {
            uint16_t held=0u;
            if(pad.held&SAT_PAD_UP) held|=SB_UP;
            if(pad.held&SAT_PAD_DOWN) held|=SB_DOWN;
            if(pad.held&SAT_PAD_LEFT) held|=SB_LEFT;
            if(pad.held&SAT_PAD_RIGHT) held|=SB_RIGHT;
            if(pad.held&SAT_PAD_A) held|=SB_JUMP;
            while(steps--) {
                events|=sb_tick(&g_game,held,pressed,fx,fz,rx,rz);
                pressed=0u; /* A pressed edge is delivered once, never per catch-up step. */
            }
        }
        sound_event(events);
        g_frame=now;
        if (events & SB_EVENT_FALL) {
            g_camera_anchor=(sat_vec3_t){g_game.x,g_game.y,g_game.z};
        } else {
            g_camera_anchor.x+=(g_game.x-g_camera_anchor.x)/4;
            g_camera_anchor.y+=(g_game.y-g_camera_anchor.y)/6;
            g_camera_anchor.z+=(g_game.z-g_camera_anchor.z)/4;
        }
        g_eye=(sat_vec3_t){g_camera_anchor.x-sb_mul(fx,SB_F(33)),
             g_camera_anchor.y+SB_F(29),g_camera_anchor.z-sb_mul(fz,SB_F(43))};
        g_target=(sat_vec3_t){g_camera_anchor.x+sb_mul(fx,SB_F(10)),
             g_camera_anchor.y+SB_F(5),g_camera_anchor.z+sb_mul(fz,SB_F(12))};
        {
            sat_mat4_t view,projection;
            sat_example_must(sat_mat4_look_at(&view,&g_eye,&g_target,&up));
            sat_example_must(sat_mat4_perspective(&projection,SB_F(55),
                sat_fx16_div(SB_F(W),SB_F(H)),SB_F(2),SB_F(250)));
            sat_example_must(sat_mat4_multiply(&g_vp,&projection,&view));
        }
        /* VBlank: configure VDP2 before submitting the next VDP1 frame. */
        sky_scroll.x_integer=(uint16_t)(((uint32_t)g_yaw*SKY_W/360u+(g_frame>>3u))&511u);
        sat_example_must(sat_vdp2_nbg0_set_scroll(&sky_scroll));
        update_rotation(fx,fz);
        sat_example_must(sat_vdp2_layers_commit());
        sat_example_must(sat_vdp1_set_erase_transparent());
        sat_example_must(sat_begin_frame());
        draw_world(fx,fz);
        hud();
        sat_example_must(sat_end_frame());
        sat_example_must(sat_audio_update());
    }
}
