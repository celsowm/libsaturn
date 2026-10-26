#ifndef CUBE_BENCH_MESH_DATA_H
#define CUBE_BENCH_MESH_DATA_H
#include "saturn/skin3d.h"

/* Shared surface vertices: V=6*n*n+2, F=6*n*n. Eight vertices at n=1.
 * Winding and corner bit numbering match sat_mesh_build_box. */
static const uint8_t cube_topology[6][4]={{6,7,5,4},{3,2,0,1},{7,3,1,5},
    {2,6,4,0},{2,3,7,6},{4,5,1,0}};
static const int8_t cube_normals[6][3]={{0,0,1},{0,0,-1},{1,0,0},{-1,0,0},{0,1,0},{0,-1,0}};
static inline int cube_corner(unsigned v,unsigned axis,unsigned n) {return v&(1u<<axis)?(int)n:0;}

static inline sat_result_t cube_mesh_data(unsigned n,sat_skin_rigid_t* anchors,
    sat_vec3_t* vertices,unsigned vertex_capacity,uint16_t* indices,
    uint16_t* materials,unsigned face_capacity,uint16_t* grid,unsigned grid_capacity) {
    if(n<1 || n>12 || !anchors || !vertices || !indices || !materials || !grid)
        return SAT_ERR_INVALID_ARG;
    if(vertex_capacity<6*n*n+2 || face_capacity<6*n*n || grid_capacity<(n+1)*(n+1)*(n+1))
        return SAT_ERR_CAPACITY;
    unsigned vertex=0;
    for(unsigned z=0;z<=n;++z)for(unsigned y=0;y<=n;++y)for(unsigned x=0;x<=n;++x) {
        if(x && x<n && y && y<n && z && z<n)continue;
        const unsigned xyz[3]={x,y,z};
        for(unsigned axis=0;axis<3;++axis)
            anchors[vertex].pos[axis]=(int16_t)(-16384+32768*(int)xyz[axis]/(int)n);
        anchors[vertex].joint=0;anchors[vertex].reserved=0;
        vertices[vertex].x=anchors[vertex].pos[0]*2;
        vertices[vertex].y=anchors[vertex].pos[1]*2;
        vertices[vertex].z=anchors[vertex].pos[2]*2;
        grid[(z*(n+1)+y)*(n+1)+x]=(uint16_t)vertex++;
    }
    if(vertex!=6*n*n+2)return SAT_ERR_VERIFY_FAILED;
    for(unsigned side=0;side<6;++side)for(unsigned y=0;y<n;++y)for(unsigned x=0;x<n;++x) {
        const unsigned f=side*n*n+y*n+x;
        const unsigned cx[4]={x,x+1,x+1,x},cy[4]={y,y,y+1,y+1};
        for(unsigned c=0;c<4;++c) {
            unsigned xyz[3];
            for(unsigned axis=0;axis<3;++axis) {
                const int a=cube_corner(cube_topology[side][0],axis,n);
                const int b=cube_corner(cube_topology[side][1],axis,n);
                const int d=cube_corner(cube_topology[side][3],axis,n);
                xyz[axis]=(unsigned)(a+(b-a)*(int)cx[c]/(int)n+(d-a)*(int)cy[c]/(int)n);
            }
            indices[f*4+c]=grid[(xyz[2]*(n+1)+xyz[1])*(n+1)+xyz[0]];
        }
        materials[f]=(uint16_t)side;
    }
    return SAT_OK;
}
#endif
