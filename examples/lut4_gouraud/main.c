/* Official VDP1 color mode 1: RGB CLUT texture plus Gouraud corrections. */
#include "saturn/saturn.h"
#include "saturn/app.h"
#include "saturn/font.h"
#include "example_util.h"
enum { SCREEN_WIDTH=320, SCREEN_HEIGHT=224, TEXTURE_SIDE=64,
       TEXTURE_BYTES=TEXTURE_SIDE*TEXTURE_SIDE/2, ANIMATION_PERIOD=240,
       PANEL_TOP=60, PANEL_BOTTOM=180, PANEL_WIDTH=120, PANEL_GAP=24 };
static uint8_t pixels[TEXTURE_BYTES];
static sat_vdp1_texture_t texture;
static sat_ascii_font_t font;
volatile struct { uint32_t frames, shaded_draws, errors, paused; } g_lut4_metrics;
static void label(const char* value,int x,int y) {
    sat_example_must(sat_ascii_font_draw_text_screen_indexed8(&font,value,x,y,8,0,0));
}
static sat_distorted_sprite_cmd_t panel(int left) {
    sat_distorted_sprite_cmd_t cmd={0};cmd.texture=&texture;
    cmd.x[0]=cmd.x[3]=left-SCREEN_WIDTH/2;
    cmd.x[1]=cmd.x[2]=left+PANEL_WIDTH-SCREEN_WIDTH/2;
    cmd.y[0]=cmd.y[1]=PANEL_TOP-SCREEN_HEIGHT/2;
    cmd.y[2]=cmd.y[3]=PANEL_BOTTOM-SCREEN_HEIGHT/2;
    return cmd;
}
int main(void) {
    sat_example_must(sat_app_init_default());
    sat_example_must(sat_vdp1_set_frame_mode(SAT_VDP1_FRAME_ON_SUBMIT));
    uint16_t lut[16];lut[0]=0;
    for(unsigned i=1;i<16;++i)lut[i]=SAT_RGB555(6+i,6+i,6+i);
    for(unsigned y=0;y<TEXTURE_SIDE;++y)for(unsigned x=0;x<TEXTURE_SIDE;x+=2) {
        unsigned a=1+((x/8+y/8)%15),b=1+(((x+1)/8+y/8)%15);
        pixels[(y*TEXTURE_SIDE+x)/2]=(uint8_t)((a<<4)|b);
    }
    uint16_t handle;sat_example_must(sat_vdp1_upload_lut(lut,&handle));
    sat_example_must(sat_tex_upload_lut4_pixels(&texture,pixels,TEXTURE_SIDE,TEXTURE_SIDE,handle));
    sat_example_must(sat_ascii_font_init_8x8_indexed8(&font,SAT_COLOR_WHITE,0,0));
    unsigned phase=0;
    while(1) {
        sat_pad_state_t pad={0};
        sat_example_must(sat_app_frame_begin(SAT_RGB555(2,3,5),SAT_RGB555(2,3,5),&pad));
        if(pad.pressed&SAT_PAD_A)g_lut4_metrics.paused^=1;
        if(pad.pressed&SAT_PAD_B)phase=0;
        if(!g_lut4_metrics.paused)phase=(phase+1)%ANIMATION_PERIOD;
        unsigned half=ANIMATION_PERIOD/2;
        int amount=(int)((phase<half?phase:ANIMATION_PERIOD-phase)*15/half);
        uint16_t corrections[4]={sat_gouraud_rgb(amount,-4,-4),
            sat_gouraud_rgb(-4,-4,amount),sat_gouraud_rgb(-4,amount,-4),sat_gouraud_grey(0)};
        sat_distorted_sprite_cmd_t plain=panel((SCREEN_WIDTH-2*PANEL_WIDTH-PANEL_GAP)/2);
        sat_distorted_sprite_cmd_t shaded=panel((SCREEN_WIDTH+PANEL_GAP)/2);
        sat_example_must(sat_draw_sprite_distorted(&plain));
        sat_result_t result=sat_draw_sprite_distorted_gouraud(&shaded,corrections);
        if(result!=SAT_OK)++g_lut4_metrics.errors;
        sat_example_must(result);++g_lut4_metrics.shaded_draws;
        label("VDP1 LUT4 + RGB CLUT + GOURAUD",40,16);
        label("ORIGINAL",28,44);label("GOURAUD",172,44);
        label("SAME 2048-BYTE TEXTURE + 32-BYTE CLUT",16,192);
        label("A PAUSE   B RESET",88,208);
        sat_example_must(sat_app_frame_end());++g_lut4_metrics.frames;
    }
}
