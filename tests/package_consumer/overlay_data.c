/* Read-only tables for the cart overlay test: two overlay members sharing one slot, and a resident
 * cart section. -fdata-sections gives each array its own .rodata.<name> input section. */
#include <stdint.h>

const uint8_t ovl_a_table[12] = { 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A' };
const uint8_t ovl_b_table[20] = { 'B', 'B', 'B', 'B', 'B', 'B', 'B', 'B', 'B', 'B',
                                  'B', 'B', 'B', 'B', 'B', 'B', 'B', 'B', 'B', 'B' };
const uint8_t resident_table[8] = { 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R' };
