#ifndef SATURN_HAL_VDP2_VRAM_INTERNAL_HPP
#define SATURN_HAL_VDP2_VRAM_INTERNAL_HPP

/* SPDX-License-Identifier: MIT */
/* Internal extensions of the VDP2 VRAM byte free list
 * (src/hal/vdp2/allocator.cpp) for the transactional resource plan
 * (src/hal/vdp2/plan.cpp). Not part of the public API: the public surface
 * is sat_vdp2_plan_* in include/saturn/vdp2_alloc.h. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* First-fit placement of `bytes` (2-byte-word `alignment_words`) inside the
 * byte range [start_byte, end_byte) of the configured address space.
 * Returns 1 and writes the base on success; 0 when the range cannot satisfy
 * the request. The placed bytes are owned exactly as by sat_vdp2_vram_alloc
 * (free them with sat_vdp2_vram_free). */
int vram_alloc_in(uint32_t start_byte, uint32_t end_byte, uint32_t bytes,
                  uint32_t alignment_words, uint32_t* out_base);

/* Carves exactly [base_byte, base_byte + bytes) out of the free list.
 * Returns 1 on success; 0 when any of those bytes are not free or the
 * range is out of bounds. */
int vram_alloc_at(uint32_t base_byte, uint32_t bytes);

/* Bytes free within [start_byte, end_byte), clamped to the configured
 * address space. */
uint32_t vram_free_bytes(uint32_t start_byte, uint32_t end_byte);

#ifdef __cplusplus
}
#endif

#endif  /* SATURN_HAL_VDP2_VRAM_INTERNAL_HPP */
