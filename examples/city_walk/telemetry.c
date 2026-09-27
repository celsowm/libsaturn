/* city_walk telemetry: the struct the harness reads out of WRAM-H by symbol. */
#include "city_walk.h"

volatile city_telemetry_t g_city;

void telemetry_init(void) {
    volatile uint32_t* words = (volatile uint32_t*)&g_city;
    for (uint32_t i = 0u; i < sizeof(g_city) / 4u; ++i) words[i] = 0u;
    g_city.magic = CITY_TELEMETRY_MAGIC;
    g_city.state = CITY_STATE_BOOT;
}
