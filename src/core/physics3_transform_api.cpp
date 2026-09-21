#include "saturn/physics3_transform.h"
extern "C" sat_result_t sat_physics3_sync_sphere_transform(
    const sat_physics3_world_t* physics,uint16_t sphere_id,
    sat_transform3d_world_t* transforms,uint16_t node_id){
    if(!transforms||!transforms->nodes||!transforms->capacity||
       transforms->count>transforms->capacity||
       node_id>=transforms->count||
       transforms->nodes[node_id].parent!=SAT_TRANSFORM3D_ROOT)
        return SAT_ERR_INVALID_ARG;
    sat_mat4_t model{};
    const sat_result_t status=sat_physics3_sphere_model_matrix(
        physics,sphere_id,&model);
    if(status!=SAT_OK)return status;
    return sat_transform3d_set_local_matrix(transforms,node_id,&model);
}
