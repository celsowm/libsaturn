#ifndef SATURN_HAL_SMPC_HPP
#define SATURN_HAL_SMPC_HPP

#include <stdint.h>

#include "src/hal/smpc/input_logic.hpp"
#include "src/hal/smpc/peripheral_logic.hpp"
#include "src/hal/smpc/status_logic.hpp"

namespace saturn::hal::smpc {

/* One INTBACK for the ports in `port_mask` (bit 0 = port 1, bit 1 = port 2;
 * the others are put in 0-byte mode and cost nothing), followed by CONTINUE
 * requests while the SMPC reports more data (a multitap needs several 32-byte
 * chunks). Ports outside the mask come back with `parsed` false. False when
 * the SMPC never answers or the report does not parse; `parse` then says why
 * (Ok when the failure was the bus). */
bool read_peripherals(PeripheralSnapshot* out_snapshot, uint8_t port_mask = kIntbackPortsBoth,
                      ParseStatus* parse = nullptr);
/* read_peripherals in two halves, so the CPU can work while the SMPC gathers the pads (that takes
 * milliseconds: Mednafen's SMPC keeps SF set for ~2.6 ms). begin_peripheral_read issues the
 * INTBACK and returns; finish_peripheral_read waits for the first chunk, collects the rest and
 * parses, like read_peripherals. Any other SMPC command issued meanwhile abandons the pending
 * read (finish then returns false). */
bool begin_peripheral_read(uint8_t port_mask = kIntbackPortsBoth);
bool peripheral_read_pending();
bool finish_peripheral_read(PeripheralSnapshot* out_snapshot, ParseStatus* parse = nullptr);
/* INTBACK status block: RTC, cartridge and area code, SMEM. */
bool read_status(SmpcStatus* out_status);
/* SETTIME (the weekday is derived) and SETSMEM. */
bool set_time(const RtcTime& time);
bool set_smem(const uint8_t smem[4]);
bool sound_on();
bool sound_off();
/* SSHON/SSHOFF are master-owned operations.  They do not make any claim
 * about application-level slave initialization or communication state. */
bool slave_on();
bool slave_off();
bool reset_enable();
/* SYSRES: resets everything but the SMPC; the BIOS then boots the disc again. */
[[noreturn]] void system_reset();
bool reset_disable();

}  // namespace saturn::hal::smpc

#endif
