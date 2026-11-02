#include "saturn/texture_memory.h"
#include "src/core/memory/section.hpp"

/* The default table (saturn/texture_memory.h): a program that defines `sat_texture_memory` leaves this file, and the
 * slots below, out of the link. */
namespace {

constexpr uint16_t kTextures = 64u;
constexpr uint16_t kRegions = 64u;
constexpr uint16_t kBuckets = 128u;

sat_texture_slot_t g_textures[kTextures];
sat_texture_region_slot_t g_regions[kRegions];
uint16_t g_buckets[kBuckets];

}  // namespace

extern "C" const sat_texture_memory_t sat_texture_memory = {
    g_textures, g_regions, g_buckets, kTextures, kRegions, kBuckets};
