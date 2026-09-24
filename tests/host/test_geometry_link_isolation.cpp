/* Link only core math + mesh geometry, with no graphics/VDP1 objects. */
#include <cstdio>
#include <cstdlib>
#include "saturn/mesh3d.h"
#include "saturn/geometry3d.h"
#ifdef SATURN_VDP1_H
#error "Standalone mesh geometry unexpectedly imported VDP1"
#endif
#ifdef SATURN_RENDER3D_H
#error "Standalone mesh geometry unexpectedly imported the renderer"
#endif
#define CHECK(expr) do { if (!(expr)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); std::exit(1); } } while (0)
int main() {
    sat_vec3_t vertices[8]{};
    uint16_t indices[24]{};
    sat_mesh_t mesh{};
    CHECK(sat_mesh_init(&mesh,vertices,8u,indices,6u)==SAT_OK);
    const sat_vec3_t center{0,0,0};
    CHECK(sat_mesh_build_box(&mesh,&center,SAT_FX16_ONE,
                             SAT_FX16_ONE,SAT_FX16_ONE)==SAT_OK);
    CHECK(mesh.vertex_count==8u && mesh.face_count==6u);
    sat_quad3_t quad{};
    CHECK(sat_mesh_face_quad(&mesh,0u,&quad)==SAT_OK);
    sat_vec3_t normal{};
    CHECK(sat_mesh_face_normal_scaled(&mesh,0u,&normal)==SAT_OK);
    CHECK(normal.x!=0 || normal.y!=0 || normal.z!=0);
    sat_mat4_t matrix{};
    CHECK(sat_mat4_identity(&matrix)==SAT_OK);
    CHECK(sat_mesh_transform(&mesh,&matrix)==SAT_OK);
    CHECK(sat_mesh_face_quad(&mesh,0u,&quad)==SAT_OK);
    CHECK(sat_mesh_face_quad(&mesh,6u,&quad)==SAT_ERR_INVALID_ARG);
    std::puts("standalone geometry C API link: OK");
    return 0;
}
