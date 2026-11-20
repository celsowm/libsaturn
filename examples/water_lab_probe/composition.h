#ifndef WATER_LAB_PROBE_COMPOSITION_H
#define WATER_LAB_PROBE_COMPOSITION_H

#include "saturn/core.h"

void probe_composition_synthetic(void);
void probe_composition_init(void);
void probe_composition_release(void);
sat_result_t probe_composition_upload(uint32_t serial, const uint16_t* src, unsigned rows);
void probe_composition_publish(uint32_t serial);
uint32_t probe_composition_pair_rgb_rejected(void);

#endif
