#include <cassert>
#include <map>
#include <set>
#include <tuple>
#include <vector>
#include "examples/cube_pipeline_bench/mesh_data.h"

int main() {
    for(unsigned n=1;n<=12;++n) {
        const unsigned v=6*n*n+2,f=6*n*n,g=(n+1)*(n+1)*(n+1);
        std::vector<sat_skin_rigid_t> anchors(v);
        std::vector<sat_vec3_t> vertices(v);
        std::vector<uint16_t> indices(f*4),materials(f),grid(g);
        assert(cube_mesh_data(n,anchors.data(),vertices.data(),v,indices.data(),materials.data(),f,grid.data(),g)==SAT_OK);
        std::set<std::tuple<int32_t,int32_t,int32_t>> unique;
        std::map<std::pair<unsigned,unsigned>,unsigned> edges;
        for(unsigned i=0;i<v;++i) {
            const auto& p=vertices[i];unique.emplace(p.x,p.y,p.z);
            assert(p.x==anchors[i].pos[0]*2 && p.y==anchors[i].pos[1]*2 && p.z==anchors[i].pos[2]*2);
            assert(anchors[i].joint==0);
        }
        assert(unique.size()==v);
        for(unsigned face=0;face<f;++face) {
            assert(materials[face]==face/(n*n));
            for(unsigned c=0;c<4;++c) {
                unsigned a=indices[face*4+c],b=indices[face*4+(c+1)%4];
                assert(a<v && b<v && a!=b);if(a>b)std::swap(a,b);++edges[{a,b}];
            }
            const auto& a=vertices[indices[face*4]];const auto& b=vertices[indices[face*4+1]];
            const auto& d=vertices[indices[face*4+3]];
            const int64_t dx=d.x-a.x,dy=d.y-a.y,dz=d.z-a.z;
            const int64_t bx=b.x-a.x,by=b.y-a.y,bz=b.z-a.z;
            const int64_t normal[3]={dy*bz-dz*by,dz*bx-dx*bz,dx*by-dy*bx};
            int64_t dot=0;for(unsigned axis=0;axis<3;++axis)dot+=normal[axis]*cube_normals[materials[face]][axis];
            assert(dot>0);
        }
        for(auto edge:edges)assert(edge.second==2);
        assert(static_cast<int>(v)-static_cast<int>(edges.size())+static_cast<int>(f)==2);
        anchors[0].joint=123;
        assert(cube_mesh_data(n,anchors.data(),vertices.data(),v-1,indices.data(),materials.data(),f,grid.data(),g)==SAT_ERR_CAPACITY);
        assert(anchors[0].joint==123); // rejected before changing output
        assert(cube_mesh_data(0,anchors.data(),vertices.data(),v,indices.data(),materials.data(),f,grid.data(),g)==SAT_ERR_INVALID_ARG);
    }
}
