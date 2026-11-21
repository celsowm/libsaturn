#include "saturn/water_plane.h"

extern "C" sat_result_t sat_water_plane_submit(sat_scene_t* scene, sat_water_plane_t* plane,
    const sat_scene3d_material_t* materials, uint16_t pass) {
    if (!scene || !plane || !materials || plane->ring_count > SAT_WATER_PLANE_RINGS_MAX)
        return SAT_ERR_INVALID_ARG;
    for (unsigned k = 0; k < plane->ring_count; ++k) {
        sat_water_plane_ring_t& ring = plane->ring[k];
        if (ring.mesh.face_count == 0) continue;
        sat_scene3d_material_t material = materials[k];
        material.vertex_gouraud = ring.light;
        sat_scene3d_instance_t instance{};
        instance.mesh = &ring.mesh;
        instance.materials = &material;
        instance.material_count = 1;
        instance.face_materials = plane->face_materials;
        instance.pass = pass;
        instance.cull_backfaces = 1;
        const sat_result_t result = sat_scene_submit_instance(
            scene, &instance, SAT_SCENE3D_SLOT_INHERIT, plane->scratch, nullptr);
        if (result != SAT_OK) return result;
    }
    return SAT_OK;
}
