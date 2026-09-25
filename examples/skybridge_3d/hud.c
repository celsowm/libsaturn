/* Skybridge 3D: boot panel, HUD and debug overlay text. */
#include "skybridge.h"

static sat_ascii_font_t g_font;
static uint8_t g_show_help=1u;
uint8_t g_show_debug=0u;

void hud_font_init(void) {
    sat_example_must(sat_ascii_font_init_8x8_indexed8(
        &g_font,SAT_COLOR_WHITE,SAT_COLOR_BLACK,2u));
}

/* Boot is a series of completed jobs, not a pretend timed progress bar.
 * VDP1 draws this fullscreen panel independently of whether VDP2 is ready. */
void loading_frame(const char* stage, uint8_t percent) {
    sat_example_loading_frame(&g_font,"SKYBRIDGE 3D",stage,percent,W,H);
}
void put_text(const char* text, int x, int y) {
    (void)sat_ascii_font_draw_text_screen_indexed8(
        &g_font, text, x, y, 8, 0u, 0u);
}
void put_label(const char* title, uint32_t n, int x, int y) {
    (void)sat_ascii_font_draw_label_u32(&g_font,title,n,x,y,8,0u,0u);
}
/* Display signed 16.16 WORLD coordinates, to one decimal place. The axis
 * letter is the only part the HUD still assembles; sat_fmt_fx16 owns the
 * number, including keeping the sign on -0.x during a fall. */
static void hud_coord(char axis,int32_t fixed,int x,int y) {
    char out[2u+SAT_FMT_FX16_MAX];
    out[0]=axis;
    out[1]=' ';
    if(sat_fmt_fx16(fixed,1u,out+2,sizeof(out)-2u,0)!=SAT_OK)return;
    put_text(out,x,y);
}

/* Per-course HUD strings are tables indexed by the course, not three
 * separate ternary ladders that have to be kept in the same order. */
static const char* const k_course_complete[4]={
    "COURSE 1 COMPLETE","COURSE 2 COMPLETE",
    "COURSE 3 COMPLETE","COURSE 4 COMPLETE"};
static const char* const k_course_next[4]={
    "START: COURSE 2","START: COURSE 3",
    "START: COURSE 4","START: REPLAY"};
static const char* const k_course_hint[4]={
    "GEMS OPTIONAL  Z BRAKE","GEMS OPTIONAL  Z BRAKE",
    "JUMP THE YELLOW-RIM HOLES","RIDE THE TILTING RAMPS"};
void draw_hud(void) {
    uint8_t i,count=0u;
    for(i=0u;i<SB_PICKUP_COUNT;++i) if(g_game.pickups&(1u<<i)) ++count;
    (void)sat_draw_rect_screen(0,0,W,16u,SAT_RGB555(3,8,15));
    put_text("SKYBRIDGE",7,4);
    put_label("C",g_game.course+1u,104,4);
    put_label("GEMS ",count,144,4);
    put_text("/8",192,4);
    put_label("TIME ",g_game.ticks/60u,224,4);
    /* Always show player position, not the smoothed camera anchor. */
    (void)sat_draw_rect_screen(0,17,W,13u,SAT_RGB555(3,8,15));
    hud_coord('X',g_game.x,7,19);
    hud_coord('Y',g_game.y,112,19);
    hud_coord('Z',g_game.z,217,19);
    if(g_show_debug) {
        /* Y toggles the extra camera-orbit diagnostic below the XYZ row. */
        uint8_t surface=sb_ground_surface(&g_game,g_game.support);
        (void)sat_draw_rect_screen(0,31,W,13u,SAT_RGB555(3,8,15));
        put_label("DECK ",g_game.support<0?0u:(uint32_t)g_game.support+1u,7,33);
        put_label("YAW ",(uint32_t)g_yaw,101,33);
        if(g_game.support>=0 &&
           sb_course_platforms(&g_game)[(uint8_t)g_game.support].kind==SB_SEESAW)
            hud_coord('T',g_game.seesaw_tilt[(uint8_t)g_game.support],197,33);
        else
            put_text(surface==SB_SURFACE_SLICK?"ICE":
                     surface==SB_SURFACE_GRIP?"GRIP":
                     surface==SB_SURFACE_AIR?"AIR":"NORMAL",197,33);
        /* Budget row: the two limits that silently eat geometry. F is the
         * painter queue, C the VDP1 command list, P how many of the character's
         * faces actually reached the queue. Either budget at its cap explains a
         * half-drawn frame; all three well under it means the cause is paint
         * ORDER, not budget. The library lays the cells out, so adding a field
         * cannot make two of them overlap into an unreadable number. */
        const sat_debug_field_t budget[3]={
            {"F ",g_dbg_faces,g_dbg_face_cap},
            {"C ",g_dbg_cmds,g_dbg_cmd_cap},
            {"P ",g_dbg_pig_faces,SKYBRIDGE_PIG_FACE_COUNT}};
        (void)sat_draw_rect_screen(0,45,W,13u,SAT_RGB555(3,8,15));
        (void)sat_ascii_font_draw_fields(
            &g_font,budget,3u,7,47,W-14u,8,0u,0u);
        if(g_dbg_world_full) put_text("FULL",270,47);
        (void)sat_draw_rect_screen(0,59,W,13u,SAT_RGB555(3,8,15));
        put_label("MS ",g_metrics.frame_cpu_ms,7,61);
        put_label("WAIT ",g_metrics.wait_ms,66,61);
        put_label("TASK ",g_metrics.task_count,151,61);
        put_label("ERR ",g_metrics.failures,244,61);
    }
    if (g_game.finished) {
        (void)sat_draw_rect_screen(46,76,228u,75u,SAT_RGB555(2,13,16));
        put_text(k_course_complete[g_game.course&3u],66,83);
        put_label("GEMS ",count,116,104);
        put_text("/8",164,104);
        put_text(k_course_next[g_game.course&3u],82,128);
    } else if (g_game.paused) {
        put_text("START: PLAY COURSE",80,92);
        put_text("X: NEXT COURSE",88,108);
    } else if(g_show_help && g_game.ticks<480u) {
        put_text(k_course_hint[g_game.course&3u],8,192);
        put_text("D-PAD MOVE  A JUMP",8,204);
        put_text("B/C CAMERA  START PAUSE",8,215);
    }
}
