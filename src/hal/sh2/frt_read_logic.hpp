#ifndef SATURN_HAL_SH2_FRT_READ_LOGIC_HPP
#define SATURN_HAL_SH2_FRT_READ_LOGIC_HPP
#include <stdint.h>
namespace saturn::hal::sh2::frt {
/* An IRQ can read FRC between our high and low loads and replace TEMP.
 * If the high byte changed, the low byte cannot be paired with the old high.
 * Retry without masking interrupts or changing their priority. */
template <typename High,typename Low>
inline uint16_t read_consistent(High high,Low low) {
    for (;;) {
        const uint8_t before=high();
        const uint8_t lower=low();
        if(before==high())return static_cast<uint16_t>((before<<8u)|lower);
    }
}
}
#endif
