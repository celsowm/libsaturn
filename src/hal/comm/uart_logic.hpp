#ifndef SATURN_HAL_COMM_UART_LOGIC_HPP
#define SATURN_HAL_COMM_UART_LOGIC_HPP

#include <stdint.h>

/* Register layout and arithmetic of the 16550-family UART (the NS16550A the
 * NetLink modem cartridge is built around, and every compatible). Pure: no
 * access to hardware. */
namespace saturn::hal::comm::uart_logic {

/* Register indices (times the bus stride for the offset). With DLAB set in LCR
 * indices 0 and 1 are the divisor latch instead of RBR/THR and IER. */
constexpr uint8_t kRbrThr = 0u;   /* receive buffer / transmit holding; DLL with DLAB */
constexpr uint8_t kIerDlm = 1u;   /* interrupt enable; DLM with DLAB */
constexpr uint8_t kIirFcr = 2u;   /* interrupt identification / FIFO control */
constexpr uint8_t kLcr = 3u;
constexpr uint8_t kMcr = 4u;
constexpr uint8_t kLsr = 5u;
constexpr uint8_t kMsr = 6u;
constexpr uint8_t kScr = 7u;

constexpr uint8_t kLcrDlab = 0x80u;
constexpr uint8_t kFcrEnableClear = 0x07u;   /* FIFO enable, clear receive and transmit */
constexpr uint8_t kIirFifoMask = 0xC0u;      /* both bits set: a working FIFO (16550A) */
constexpr uint8_t kMcrLoopback = 0x10u;
constexpr uint8_t kMcrDtrRts = 0x03u;

constexpr uint8_t kLsrDataReady = 0x01u;
constexpr uint8_t kLsrOverrun = 0x02u;
constexpr uint8_t kLsrParity = 0x04u;
constexpr uint8_t kLsrFraming = 0x08u;
constexpr uint8_t kLsrBreak = 0x10u;
constexpr uint8_t kLsrThrEmpty = 0x20u;
constexpr uint8_t kLsrErrors = kLsrOverrun | kLsrParity | kLsrFraming | kLsrBreak;

/* The divisor for a clock and baud rate, rounded to nearest; 0 when it does not
 * fit the 16-bit latch or an input is 0. The UART samples at 16x the baud rate. */
constexpr uint32_t divisor(uint32_t clock_hz, uint32_t baud) {
    if (clock_hz == 0u || baud == 0u) return 0u;
    const uint64_t rate = static_cast<uint64_t>(baud) * 16u;
    const uint64_t value = (static_cast<uint64_t>(clock_hz) + rate / 2u) / rate;
    return (value == 0u || value > 0xFFFFu) ? 0u : static_cast<uint32_t>(value);
}

/* LCR word format: data bits 5-8, stop bits 1 or 2, parity 0 none / 1 odd / 2 even. 0xFF when invalid. */
constexpr uint8_t line_control(uint8_t data_bits, uint8_t stop_bits, uint8_t parity) {
    if (data_bits < 5u || data_bits > 8u || (stop_bits != 1u && stop_bits != 2u) || parity > 2u) return 0xFFu;
    uint8_t lcr = static_cast<uint8_t>(data_bits - 5u);
    if (stop_bits == 2u) lcr |= 0x04u;
    if (parity == 1u) lcr |= 0x08u;
    if (parity == 2u) lcr |= 0x18u;
    return lcr;
}

}  // namespace saturn::hal::comm::uart_logic

#endif  // SATURN_HAL_COMM_UART_LOGIC_HPP
