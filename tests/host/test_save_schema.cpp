#include <cstdio>
#include <cstdlib>

#include "saturn/save_schema.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); return 1; } } while (0)

int main() {
    struct payload { uint32_t counter; uint32_t seed; } value{1u, 0x13579BDFu};
    sat_save_schema_header_t header{};
    uint32_t wire_size = 0u;
    OK(sat_save_schema_requirements(sizeof(value), &wire_size) == SAT_OK);
    OK(wire_size == sizeof(header) + sizeof(value));
    OK(sat_save_schema_encode(&header, 0x4C534156u, 1u, &value, sizeof(value)) == SAT_OK);

    uint32_t payload_size = 0u;
    OK(sat_save_schema_validate(&header, 0x4C534156u, 1u,
                                &value, sizeof(value), &payload_size) == SAT_OK);
    OK(payload_size == sizeof(value));
    OK(sat_save_schema_validate(&header, 0x4C534156u, 2u,
                                &value, sizeof(value), &payload_size) == SAT_ERR_VERSION);
    value.counter = 2u;
    OK(sat_save_schema_validate(&header, 0x4C534156u, 1u,
                                &value, sizeof(value), &payload_size) == SAT_ERR_VERIFY_FAILED);
    std::puts("save schema: OK");
    return 0;
}
