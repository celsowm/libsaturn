#include <cstdio>
#include "saturn/resource_plan.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); return 1; } } while (0)

int main() {
    sat_resource_plan_entry_t entries[2] = {};
    sat_resource_plan_t plan{};
    uint32_t bytes = 0u;
    OK(sat_resource_plan_requirements(2u, &bytes) == SAT_OK);
    OK(bytes == sizeof(entries));
    OK(sat_resource_plan_init(&plan, entries, 2u) == SAT_OK);
    OK(sat_resource_plan_set_limit(&plan, SAT_RESOURCE_MAIN_RAM, 100u) == SAT_OK);
    OK(sat_resource_plan_add(&plan, SAT_RESOURCE_MAIN_RAM, 64u, 4u, 1u) == SAT_OK);
    OK(entries[0].offset == 0u);
    OK(sat_resource_plan_add(&plan, SAT_RESOURCE_MAIN_RAM, 40u, 4u, 0u) == SAT_ERR_BUSY);
    OK(sat_resource_plan_add(&plan, SAT_RESOURCE_WRAM, 16u, 3u, 1u) == SAT_ERR_INVALID_ARG);
    OK(sat_resource_plan_finalize(&plan) == SAT_OK);
    OK(sat_resource_plan_add(&plan, SAT_RESOURCE_WRAM, 1u, 1u, 1u) == SAT_ERR_INVALID_ARG);
    uint32_t limit = 0u;
    OK(sat_resource_plan_usage(&plan, SAT_RESOURCE_MAIN_RAM, &bytes, &limit) == SAT_OK);
    OK(bytes == 64u && limit == 100u);
    std::puts("resource plan: OK");
    return 0;
}
