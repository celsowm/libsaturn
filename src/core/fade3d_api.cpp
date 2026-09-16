#include "saturn/fade3d.h"

#include "src/core/fade3d_logic.hpp"

extern "C" sat_result_t sat_fade3d_eval(
    const sat_fade3d_t* fade,
    sat_fx16_t view_depth,
    sat_fade3d_result_t* out
) {
    return saturn::core::fade3d::eval(fade, view_depth, out);
}
