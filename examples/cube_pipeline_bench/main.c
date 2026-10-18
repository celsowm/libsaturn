/* Procedural, stock-RAM renderer ladder. Stage 1 owns the VDP1 command table
 * and plot trigger directly; later stages add one runtime layer at a time. */
#include "saturn/saturn.h"
#include "saturn/app.h"
#include "saturn/font.h"
#include "saturn/scene.h"
#include "saturn/skin3d.h"
#include "saturn/time.h"
#include "example_util.h"
#include "mesh_data.h"

#ifndef CUBE_BENCH_STAGE
#define CUBE_BENCH_STAGE 1
#endif
#ifndef CUBE_BENCH_DIV
#define CUBE_BENCH_DIV 1
#endif
#ifndef CUBE_BENCH_HUD
#define CUBE_BENCH_HUD 1
#endif
#if CUBE_BENCH_STAGE < 0 || CUBE_BENCH_STAGE > 9 || CUBE_BENCH_DIV < 1 || CUBE_BENCH_DIV > 12
#error "stage must be 0..9; subdivisions must be 1..12"
#endif

enum { W=320,H=224,N=CUBE_BENCH_DIV,V=6*N*N+2,F=6*N*N,
       CAP=2*F,ROWS=1024,OVERLAY=128,RAW_CAP=CAP+OVERLAY+3 };
#define FX(n) ((sat_fx16_t)((n)*SAT_FX16_ONE))
#define RAM_LOW __attribute__((section(".wram_l")))
typedef struct {
    uint32_t serial,field_begin,field_end,wait,begin,animation,skin,
        geometry,flush,hud,present,work,commands,faces,draw_waits,draw_timeouts,plot;
} bench_sample_t;
typedef struct {
    uint32_t magic,version,stage,divisions,hud,frequency,count,cursor,serial,
        errors,timeouts,field,vertices_per_actor,faces_per_actor,native_faces,slave_tasks;
    bench_sample_t samples[ROWS];
} bench_results_t;
volatile bench_results_t g_cube_bench;

typedef struct {
    sat_vec3_t vertices[V],world_vertices[V];
    sat_projected_vertex_t projected[V];
    sat_mesh_t mesh;
    sat_mat4_t world,joint;
    sat_scene3d_material_t materials[6];
    sat_scene3d_instance_t instance;
    sat_scene3d_instance_binding_t binding;
} actor_t;
static actor_t actors[2] RAM_LOW;
static uint16_t indices[4*F],face_materials[F];
static sat_skin_rigid_t anchors[V];
static sat_skin_asset_t skin;
static sat_scene_t scene;
static sat_scene3d_face_t faces[CAP] RAM_LOW,worker_faces[F] RAM_LOW;
static uint32_t keys[CAP] RAM_LOW,worker_keys[F] RAM_LOW;
static uint16_t order[CAP] RAM_LOW;
static sat_scene3d_prepare_item_t worker_item;
static sat_scene3d_prepare_batch_t worker_batch;
static sat_camera3d_t camera;
static sat_ascii_font_t font;
static sat_vdp1_texture_t texture;
static uint16_t native_actor[CAP],native_face[CAP],native_count;
static uint32_t native_depth[CAP];
static sat_vec3_t local_vertices[V];
static uint16_t vertex_grid[(N+1)*(N+1)*(N+1)];

/* VDP1 manual chapters 4, 5 and 6: each command is 16 halfwords; VRAM is
 * cache-through and 32-byte aligned. No SDK submission/draw calls in stage 1. */
typedef union {
    struct {
        uint16_t ctrl,link,pmod,colr,srca,size;
        int16_t xa,ya,xb,yb,xc,yc,xd,yd;
        uint16_t grda,pad;
    } c;
    uint32_t words[8];
} raw_command_t;
_Static_assert(sizeof(raw_command_t)==32,"VDP1 command ABI");
_Static_assert(sizeof(bench_sample_t)==68,"benchmark telemetry ABI");
static raw_command_t raw_commands[RAW_CAP];
static unsigned raw_count;
#define RAW_PTMR (*(volatile uint16_t*)0x25D00004u)
#define RAW_EDSR (*(volatile uint16_t*)0x25D00010u)
#define RAW_VRAM ((volatile uint32_t*)0x25C00000u)

static void must(sat_result_t r) {
    if(r!=SAT_OK)++g_cube_bench.errors;
    sat_example_must(r);
}
static uint32_t now(void) {return (uint32_t)sat_time_ticks();}
static raw_command_t* raw_append(void) {
    if(raw_count>=RAW_CAP)must(SAT_ERR_CAPACITY);
    raw_command_t* out=&raw_commands[raw_count++];*out=(raw_command_t){0};return out;
}
static void raw_begin(void) {
    raw_count=0;
    raw_command_t* c=raw_append();c->c.ctrl=0x000A;c->c.xa=W/2;c->c.ya=H/2;
    c=raw_append();c->c.ctrl=0x0009;c->c.xc=W-1;c->c.yc=H-1;
}
static void polygon(const int16_t x[4],const int16_t y[4],uint16_t color) {
    if(CUBE_BENCH_STAGE==1) {
        raw_command_t* c=raw_append();c->c.ctrl=0x0004;c->c.pmod=0x00C0;c->c.colr=color;
        c->c.xa=x[0];c->c.ya=y[0];c->c.xb=x[1];c->c.yb=y[1];
        c->c.xc=x[2];c->c.yc=y[2];c->c.xd=x[3];c->c.yd=y[3];
    } else {
        sat_polygon_cmd_t c={{x[0],x[1],x[2],x[3]},
            {y[0],y[1],y[2],y[3]},color,0};
        must(sat_vdp1_draw_polygon(&c));
    }
}
static void text(const char* s,int x,int y) {
    if(!CUBE_BENCH_HUD)return;
    if(CUBE_BENCH_STAGE!=1) {
        must(sat_ascii_font_draw_text_screen_indexed8(&font,s,x,y,8,0,0));return;
    }
    for(;*s;++s,x+=8) {
        const unsigned ch=(uint8_t)*s;
        if(ch<32 || ch>127)continue;
        const sat_vdp1_texture_t* t=&font.glyphs[ch-32];
        raw_command_t* c=raw_append();c->c.ctrl=0;c->c.pmod=0x00A0;
        c->c.srca=t->srca;c->c.size=((t->width/8)<<8)|t->height;
        c->c.colr=0;c->c.xa=x-W/2;c->c.ya=y-H/2;
    }
}
static void number(const char* label,uint32_t value,int x,int y) {
    char digits[11],out[12];unsigned n=0;
    do {digits[n++]=(char)('0'+value%10);value/=10;}while(value);
    for(unsigned i=0;i<n;++i)out[i]=digits[n-1-i];
    out[n]=0;
    text(label,x,y);while(*label++){x+=8;}text(out,x,y);
}
static void raw_present(bench_sample_t* sample) {
    raw_append()->c.ctrl=0x8000;
    /* Every call joins its own plot before returning, so the next upload is
     * safe even after swaps have cleared CEF/BEF. EDSR is not a durable fence. */
    for(unsigned c=0;c<raw_count;++c)
        for(unsigned w=0;w<8;++w)RAW_VRAM[c*8+w]=raw_commands[c].words[w];
    const uint32_t start=now();RAW_PTMR=1;
    while(!(RAW_EDSR&2)) {
        if(now()-start>g_cube_bench.frequency/4) {
            ++g_cube_bench.timeouts;must(SAT_ERR_TIMEOUT);
        }
    }
    sample->plot=now()-start;
}

static void build_mesh(void) {
    must(cube_mesh_data(N,anchors,local_vertices,V,indices,face_materials,F,
        vertex_grid,(N+1)*(N+1)*(N+1)));
    skin=(sat_skin_asset_t){anchors,NULL,NULL,V,0,1,15,0};must(sat_skin_validate(&skin));
}
static void project(unsigned actor) {
    actor_t* a=&actors[actor];
    /* Translation only, exactly the same object transform used by scene. */
    for(unsigned v=0;v<V;++v) {
        a->world_vertices[v]=a->vertices[v];a->world_vertices[v].x+=a->world.m[3];
    }
    must(sat_project_vertices(&camera.view_proj,a->world_vertices,V,a->projected));
}
static void cache_native_order(void) {
    native_count=0;
    for(unsigned actor=0;actor<2;++actor) {
        project(actor);
        actor_t* a=&actors[actor];
        for(unsigned f=0;f<F;++f) {
            const unsigned side=f/(N*N);const int8_t* n=cube_normals[side];
            const int64_t facing=(int64_t)n[0]*(camera.eye.x-a->world.m[3]-n[0]*FX(1)/2)+
                (int64_t)n[1]*(camera.eye.y-n[1]*FX(1)/2)+
                (int64_t)n[2]*(camera.eye.z-n[2]*FX(1)/2);
            if(facing<=0)continue;
            const uint16_t* ix=&indices[f*4];const sat_projected_vertex_t* p=a->projected;
            const int area=(p[ix[2]].x-p[ix[0]].x)*(p[ix[3]].y-p[ix[1]].y)-
                (p[ix[2]].y-p[ix[0]].y)*(p[ix[3]].x-p[ix[1]].x);
            if(area<=0)continue;
            int64_t depth=0;
            for(unsigned c=0;c<4;++c) {
                sat_vec3_t v=a->world_vertices[ix[c]];
                depth+=((int64_t)(v.x-camera.eye.x)*scene.faces.forward.x+
                    (int64_t)(v.y-camera.eye.y)*scene.faces.forward.y+
                    (int64_t)(v.z-camera.eye.z)*scene.faces.forward.z)>>16;
            }
            const uint32_t key=(uint32_t)(depth/4/16);
            unsigned at=native_count++;
            while(at && native_depth[at-1]<key) {
                native_depth[at]=native_depth[at-1];native_actor[at]=native_actor[at-1];
                native_face[at]=native_face[at-1];--at;
            }
            native_depth[at]=key;native_actor[at]=actor;native_face[at]=f;
        }
    }
}
static void native_draw(void) {
    for(unsigned i=0;i<native_count;++i) {
        const unsigned f=native_face[i];actor_t* a=&actors[native_actor[i]];
        int16_t x[4],y[4];for(unsigned c=0;c<4;++c) {
            const sat_projected_vertex_t* p=&a->projected[indices[f*4+c]];x[c]=p->x;y[c]=p->y;
        }
        polygon(x,y,a->materials[face_materials[f]].rgb555);
    }
}
static void submit_actor(unsigned actor) {
    actor_t* a=&actors[actor];
    must(sat_scene_submit_instance(&scene,&a->instance,SAT_SCENE3D_SLOT_INHERIT,a->projected,a->world_vertices));
}
static void scene_geometry(void) {
    if(CUBE_BENCH_STAGE>=8 && sat_parallel_slave_available()) {
        actor_t* a=&actors[1];
        worker_item=(sat_scene3d_prepare_item_t){&a->instance,a->projected,a->world_vertices,SAT_SCENE3D_SLOT_INHERIT,0};
        must(sat_scene3d_prepare_batch_init(&worker_batch,&worker_item,1,worker_faces,worker_keys,F));
        worker_batch.dispatch=SAT_SCENE3D_PREPARE_DISPATCH_RUNTIME;
        sat_parallel_handle_t handle;must(sat_scene_prepare_batch_async(&scene,&worker_batch,&handle));
        submit_actor(0);must(sat_parallel_wait(handle,60000));
        must(sat_scene_merge_prepared_batch(&scene,&worker_batch,handle));
        must(sat_scene_prepare_batch_release(&scene,&worker_batch,handle));++g_cube_bench.slave_tasks;
    } else {submit_actor(0);submit_actor(1);}
}
static void initialize(void) {
    must(sat_app_init_default());
    g_cube_bench.magic=0x43334231;g_cube_bench.version=1;g_cube_bench.stage=CUBE_BENCH_STAGE;
    g_cube_bench.divisions=N;g_cube_bench.hud=CUBE_BENCH_HUD;g_cube_bench.frequency=sat_time_frequency_hz();
    g_cube_bench.vertices_per_actor=V;g_cube_bench.faces_per_actor=F;
    must(sat_ascii_font_init_8x8_indexed8(&font,SAT_COLOR_WHITE,0,0));
    uint8_t pixels[32];uint16_t palette[16]={0};palette[1]=SAT_RGB555(8,16,26);palette[2]=SAT_RGB555(24,26,29);
    for(unsigned y=0;y<8;++y)for(unsigned x=0;x<4;++x)pixels[y*4+x]=(x+y)&1?0x11:0x22;
    uint16_t lut;must(sat_vdp1_upload_lut(palette,&lut));must(sat_tex_upload_lut4_pixels(&texture,pixels,8,8,lut));
    build_mesh();
    sat_vec3_t eye={0,FX(2),-FX(5)},target={0,0,0},up={0,FX(1),0};
    must(sat_camera3d_init(&camera,&eye,&target,&up,FX(45),W*SAT_FX16_ONE/H,FX(1)/8,FX(20)));
    for(unsigned i=0;i<2;++i) {
        actor_t* a=&actors[i];must(sat_mesh_init(&a->mesh,a->vertices,V,indices,F));
        a->mesh.vertex_count=V;a->mesh.face_count=F;
        __builtin_memcpy(a->vertices,local_vertices,sizeof(local_vertices));
        must(sat_mat4_translate(&a->world,(i?1:-1)*FX(6)/5,0,0));must(sat_mat4_identity(&a->joint));
        for(unsigned side=0;side<6;++side) {
            sat_scene3d_material_t* m=&a->materials[side];
            const unsigned shade=12+side*2;
            m->kind=CUBE_BENCH_STAGE==5 || CUBE_BENCH_STAGE>=7?SAT_SCENE3D_INDEXED_TEXTURED:SAT_SCENE3D_RGB;
            m->rgb555=i?SAT_RGB555(shade,shade/2,3):SAT_RGB555(3,shade/2,shade);
            m->texture=&texture;m->color_calc_slot=SAT_INDEXED_SOLID_OPAQUE;
        }
        a->instance=(sat_scene3d_instance_t){0};a->instance.mesh=&a->mesh;
        a->instance.materials=a->materials;a->instance.material_count=6;
        a->instance.face_materials=face_materials;a->instance.world=&a->world;a->instance.cull_backfaces=1;
        must(sat_scene3d_instance_binding_init(&a->binding,&a->mesh,a->materials,6,face_materials));
        a->instance.validated_binding=&a->binding;
    }
    must(sat_scene_init(&scene,faces,keys,order,CAP));
    must(sat_begin_frame());must(sat_scene_begin(&scene,&camera,FX(1)/8,W,H,OVERLAY));
    cache_native_order();g_cube_bench.native_faces=native_count;must(sat_scene_flush(&scene));must(sat_end_frame());
    if(CUBE_BENCH_STAGE>=8) {
        sat_parallel_config_t config={SAT_PARALLEL_AUTO,NULL,0,0,60000};must(sat_parallel_init(&config));
        if(!sat_parallel_slave_available())must(SAT_ERR_UNSUPPORTED);
    }
    if(CUBE_BENCH_STAGE==1) {
        must(sat_wait_vblank());RAW_PTMR=0;must(sat_wait_vblank());
    }
}
int main(void) {
    initialize();
    while(1) {
        bench_sample_t s={0};const uint32_t before_wait=now();
        must(sat_wait_vblank());const uint32_t work_start=now();
        s.wait=work_start-before_wait;s.field_begin=sat_frame_count();
        uint32_t t=now();
        if(CUBE_BENCH_STAGE==1)raw_begin();else must(sat_begin_frame());
        if(CUBE_BENCH_STAGE>=4)must(sat_scene_begin(&scene,&camera,FX(1)/8,W,H,OVERLAY));
        s.begin=now()-t;
        t=now();if(CUBE_BENCH_STAGE==9)for(unsigned i=0;i<2;++i)
            must(sat_mat4_rotate_y(&actors[i].joint,(s.field_begin%360)*SAT_FX16_ONE));
        s.animation=now()-t;
        t=now();if(CUBE_BENCH_STAGE>=6)for(unsigned i=0;i<2;++i)
            must(sat_skin_update_mesh(&skin,&actors[i].joint,1,&actors[i].mesh,0,NULL));
        s.skin=now()-t;
        t=now();
        if(CUBE_BENCH_STAGE==3){project(0);project(1);}
        if(CUBE_BENCH_STAGE>=4)scene_geometry();
        s.geometry=now()-t;
        t=now();if(CUBE_BENCH_STAGE>=4)must(sat_scene_flush(&scene));
        else if(CUBE_BENCH_STAGE)native_draw();
        s.flush=now()-t;
        t=now();
        number("STAGE ",CUBE_BENCH_STAGE,8,8);number("DIV ",N,224,8);
        number("FRAME ",g_cube_bench.serial+1,8,24);
        text("TWO CUBES / PROCEDURAL",72,200);s.hud=now()-t;
        sat_vdp1_command_stats_t command={0};sat_vdp1_frame_stats_t frame={0};
        if(CUBE_BENCH_STAGE!=1){must(sat_vdp1_command_stats(&command));must(sat_vdp1_frame_stats(&frame));}
        s.commands=CUBE_BENCH_STAGE==1?raw_count:command.used;
        s.faces=CUBE_BENCH_STAGE>=4?scene.flushed_faces:CUBE_BENCH_STAGE?native_count:0;
        s.draw_waits=frame.draw_waits;s.draw_timeouts=frame.draw_timeouts;
        t=now();if(CUBE_BENCH_STAGE==1)raw_present(&s);else must(sat_end_frame());s.present=now()-t;
        s.work=now()-work_start;s.field_end=sat_frame_count();s.serial=g_cube_bench.serial+1;
        g_cube_bench.samples[g_cube_bench.cursor]=s;
        g_cube_bench.cursor=(g_cube_bench.cursor+1)%ROWS;
        ++g_cube_bench.count;g_cube_bench.serial=s.serial;g_cube_bench.field=s.field_end;
    }
}
