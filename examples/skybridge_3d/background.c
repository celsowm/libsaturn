/* Skybridge 3D: VDP2 sky and RBG0 sea, and the VDP1 cloud band. */
#include "skybridge.h"

static sat_vdp1_texture_t g_cloud_texture;
static uint8_t g_cloud_pixels[SB_CLOUD_W * SB_CLOUD_H];
static uint32_t g_last_ocean_palette_step=0xFFFFFFFFu;
static uint8_t g_sky[SKY_W * SKY_H] __attribute__((section(".wram_l")));
static uint16_t g_sky_colors[256], g_sea_colors[256];
static uint16_t g_map[SAT_VDP2_NBG0_MAP_CELLS] __attribute__((section(".wram_l")));
static const sat_vdp2_rbg0_ground_config_t g_ocean = {
    512u, 256u, 160u, HORIZON, 96u, 8u, 96u, COEF_WORD
};
static sat_vdp2_ground_environment_t g_environment;
static uint16_t g_environment_coefficients[H * 2u];
static uint16_t g_environment_params[48];

void init_cloud_texture(void) {
    uint16_t palette[256];
    uint16_t x,y;
    for(x=0u;x<256u;++x)palette[x]=SAT_BGR555(0,0,0);
    palette[1u]=SAT_RGB555(31,31,31);
    palette[2u]=SAT_RGB555(28,30,31);
    palette[3u]=SAT_RGB555(23,27,30);
    palette[4u]=SAT_RGB555(18,24,28);
    for(y=0u;y<SB_CLOUD_H;++y)for(x=0u;x<SB_CLOUD_W;++x)
        g_cloud_pixels[y*SB_CLOUD_W+x]=sb_scenery_cloud_pixel(x,y);
    /* CRAM bank 7 is deliberately separate from sea 0, sky 1,
     * font 2, stage 3/5/6 and the fade-material bank 4. */
    sat_example_must(sat_tex_upload_indexed8(
        &g_cloud_texture,g_cloud_pixels,SB_CLOUD_W,SB_CLOUD_H,palette,7u));
}
void draw_clouds(void) {
    static const uint16_t base_x[6]={18u,102u,206u,315u,405u,488u};
    static const uint8_t y[6]={21u,42u,27u,15u,48u,32u};
    static const uint8_t width[6]={68u,52u,79u,60u,55u,72u};
    static const uint8_t height[6]={15u,12u,18u,14u,12u,16u};
    uint8_t i,copy;
#if SAT_SKYBRIDGE_VALIDATION
    /* Clouds are world VDP1 commands, so the cross-profile scene hash would
     * otherwise compare display-frame pacing instead of rendering: drive
     * them from the gameplay tick every profile shares. */
    const uint32_t cloud_clock=g_game.ticks;
#else
    const uint32_t cloud_clock=g_frame;
#endif
    for(i=0u;i<6u;++i) {
        const int32_t x=sb_scenery_cloud_x(base_x[i],(uint16_t)g_yaw,cloud_clock);
        /* Each cloud is drawn at its own position and once more one full
         * 512px period to the left, so a cloud straddling the seam stays
         * whole. Wrapping every cloud at 320px instead would visibly tile
         * the sky. */
        for(copy=0u;copy<2u;++copy) {
            const int32_t cx=x-(int32_t)copy*512;
            const int32_t half=(int32_t)width[i]/2;
            if(cx+half<=0 || cx-half>=(int32_t)W || g_world_cmd_full)
                continue;
            world_ok(sat_draw_sprite_scaled_screen(
                &g_cloud_texture,(int16_t)cx,(int16_t)y[i],
                width[i],height[i],0u));
        }
    }
}
void init_sky(void) {
    uint32_t x,y;
    for(x=0u;x<256u;++x)g_sky_colors[x]=sb_scenery_sky_color(x);
    for(y=0u;y<SKY_H;++y)for(x=0u;x<SKY_W;++x)
        g_sky[y*SKY_W+x]=sb_scenery_sky_pixel(x,y);
}
static uint8_t ocean_bitmap_pixel(void* user,uint16_t x,uint16_t y) {
    (void)user;
    return sb_scenery_sea_pixel(x,y);
}
static void ocean_bitmap_progress(void* user,uint16_t rows_complete) {
    (void)user;
    if((rows_complete&15u)==0u)
        loading_frame("GENERATING OCEAN",
            (uint8_t)(15u+((uint32_t)rows_complete*60u/SB_SEA_H)));
}
void init_sea(void) {
    uint16_t row_words[SB_SEA_W/2u];
    for(uint32_t x=0u;x<256u;++x)
        g_sea_colors[x]=sb_scenery_sea_color(x&63u);
    sat_example_must(sat_vdp2_bitmap_upload_indexed8(
        SEA_WORD,SB_SEA_W,SB_SEA_H,
        ocean_bitmap_pixel,ocean_bitmap_progress,0,
        row_words,SB_SEA_W/2u));
}
void animate_sea_palette(uint32_t tick) {
    /* Palette modulation touches only eight highlight colors: 16 bytes
     * every 8 display frames, never a 128 KiB bitmap or the fade registers.
     * The RBG0 scroll supplies flow and this supplies subtle foam shimmer. */
    uint32_t phase=(tick>>3u)&31u;
    uint32_t light=phase<16u?phase:31u-phase;
    uint32_t i;
    if(g_last_ocean_palette_step==phase)return;
    g_last_ocean_palette_step=phase;
    for(i=0u;i<8u;++i) {
        uint32_t t=i+(light>>2u);
        g_sea_colors[48u+i]=sb_scenery_rgb(6u+t/3u,18u+t/2u,
                                           24u+t/4u);
    }
    sat_example_must(sat_vdp2_palette_upload(
        &g_sea_colors[48u],8u,48u));
}
void update_rotation(int32_t fx,int32_t fz) {
    uint16_t* p = g_environment_params;
    /* Keep the sea under a fixed 96px horizon, rotate/scroll sample plane. */
    /* Two slow, non-identical currents slide the textured water plane
     * underneath a fixed world horizon without any per-frame bitmap upload. */
    sat_example_must(sat_vdp2_ground_environment_build_params(&g_environment,
        (g_game.x>>16)+(int32_t)(g_frame/9u),
        (g_game.z>>16)+(int32_t)(g_frame/17u)));
    p[15]=(uint16_t)((uint32_t)fz&0xFFFFu);
    p[17]=(uint16_t)((uint32_t)fx&0xFFFFu);
    p[21]=(uint16_t)((uint32_t)(-fx)&0xFFFFu);
    p[23]=(uint16_t)((uint32_t)fz&0xFFFFu);
    /* The parameter table uses signed 16.16 A/B/D/E, including high words. */
    p[14]=(uint16_t)(fz>>16);
    p[16]=(uint16_t)(fx>>16);
    p[20]=(uint16_t)((-fx)>>16);
    p[22]=(uint16_t)(fz>>16);
    sat_example_must(sat_vdp2_ground_environment_commit_params(&g_environment));
}
void init_background(void) {
    const sat_vdp2_nbg0_config_t sky = {
        SAT_VDP2_CHAR_SIZE_1X1,SAT_VDP2_COLOR_MODE_256,0x3Bu,0u,0u
    };
    const sat_vdp2_rbg0_mode7_config_t sea = {
        SAT_VDP2_RBG0_BITMAP_512x256,SAT_VDP2_COLOR_MODE_256,
        SEA_WORD,ROT_WORD,SAT_COLOR_BLACK,5u,7u
    };
    sat_example_must(sat_vdp2_ground_environment_validate_layout(
        &g_ocean, &sea, H, SAT_VDP2_VRAM_WORD_CAPACITY, 0u, 512u, 2u));
    sat_example_must(sat_vdp2_ground_environment_init(&g_environment,
        &g_ocean, &sea, g_environment_coefficients, H * 2u,
        g_environment_params, H));
    sat_example_must(sat_vdp2_palette_upload(g_sea_colors,256u,0u));
    sat_example_must(sat_vdp2_palette_upload(g_sky_colors,256u,256u));
    sat_example_must(sat_vdp2_nbg0_init(&sky));
    sat_example_must(sat_vdp2_nbg0_upload_indexed8(g_sky,SKY_W,SKY_H,1u,g_map));
    sat_example_must(sat_vdp2_ground_environment_upload_coefficients(&g_environment));
    sat_example_must(sat_vdp2_ground_environment_build_params(&g_environment,0,0));
    sat_example_must(sat_vdp2_ground_environment_commit_params(&g_environment));
    sat_example_must(sat_vdp2_nbg0_set_priority(2u));
    sat_example_must(sat_vdp2_rbg0_set_priority(5u));
    sat_example_must(sat_vdp2_sprite_set_priority(7u));
    sat_example_must(sat_vdp2_sprite_color_calc_configure_alpha(7u));
    loading_frame("COMPOSING VDP2",82u);
}
