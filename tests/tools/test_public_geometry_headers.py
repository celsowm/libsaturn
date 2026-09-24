"""Exercise the public C/C++ header boundary without importing VDP1 into physics."""
import subprocess

GEOMETRY = r'''
#include "saturn/geometry3d.h"
#include "saturn/mesh3d.h"
#include "saturn/collide3d.h"
#include "saturn/physics3_world.h"
#ifdef SATURN_RENDER3D_H
#error "Physics and mesh geometry must not import render3d.h"
#endif
#ifdef SATURN_VDP1_H
#error "Physics and mesh geometry must not import vdp1.h"
#endif
int main(void) {
    return (int)(sizeof(sat_quad3_t) + sizeof(sat_mesh_t)
                 + sizeof(sat_physics3_world_t));
}
'''

RENDER = r'''
#include "saturn/mesh3d_draw.h"
#ifndef SATURN_RENDER3D_H
#error "Opt-in mesh drawing must import render3d.h"
#endif
#ifndef SATURN_VDP1_H
#error "Opt-in mesh drawing must import vdp1.h"
#endif
int main(void) {
    return (int)(sizeof(sat_mesh_draw_t)
                 + sizeof(sat_indexed_solid_mesh3d_draw_t)
                 + sizeof(sat_quad3_t) + sizeof(sat_mesh_t));
}
'''

for compiler, standard in (("gcc", "c11"), ("g++", "c++20")):
    for name, source in (("geometry", GEOMETRY), ("render", RENDER)):
        result = subprocess.run(
            [compiler, "-std=" + standard, "-Wall", "-Wextra", "-Werror",
             "-Iinclude", "-x", "c" if compiler == "gcc" else "c++",
             "-fsyntax-only", "-"],
            input=source, text=True, capture_output=True, check=False,
        )
        assert result.returncode == 0, (
            f"{compiler} {name} header boundary failed:\n{result.stderr}"
        )

print("public geometry/render header isolation: OK")
