#include "saturn/uart.h"

#include "src/hal/comm/uart_logic.hpp"

namespace {

namespace logic = saturn::hal::comm::uart_logic;

constexpr uint32_t kLoopbackPolls = 4096u;

uint8_t mmio_read(void* context, uint8_t reg) {
    const sat_uart_t* uart = static_cast<const sat_uart_t*>(context);
    return *reinterpret_cast<volatile uint8_t*>(uart->base + static_cast<uintptr_t>(reg) * uart->stride);
}

void mmio_write(void* context, uint8_t reg, uint8_t value) {
    const sat_uart_t* uart = static_cast<const sat_uart_t*>(context);
    *reinterpret_cast<volatile uint8_t*>(uart->base + static_cast<uintptr_t>(reg) * uart->stride) = value;
}

inline uint8_t rd(const sat_uart_t* uart, uint8_t reg) {
    return uart->ops.read(uart->ops.context, reg);
}

inline void wr(const sat_uart_t* uart, uint8_t reg, uint8_t value) {
    uart->ops.write(uart->ops.context, reg, value);
}

void count_errors(sat_uart_t* uart, uint8_t lsr) {
    if ((lsr & logic::kLsrOverrun) != 0u) ++uart->overruns;
    if ((lsr & logic::kLsrParity) != 0u) ++uart->parity_errors;
    if ((lsr & logic::kLsrFraming) != 0u) ++uart->framing_errors;
    if ((lsr & logic::kLsrBreak) != 0u) ++uart->breaks;
}

void drain(sat_uart_t* uart) {
    for (uint32_t i = 0u; i < 32u && (rd(uart, logic::kLsr) & logic::kLsrDataReady) != 0u; ++i) {
        (void)rd(uart, logic::kRbrThr);
    }
}

// The scratch register keeps what is written, and a byte sent in loopback mode comes back.
bool responds(sat_uart_t* uart) {
    wr(uart, logic::kScr, 0x5Au);
    if (rd(uart, logic::kScr) != 0x5Au) return false;
    wr(uart, logic::kScr, 0xA5u);
    if (rd(uart, logic::kScr) != 0xA5u) return false;

    wr(uart, logic::kMcr, logic::kMcrLoopback);
    drain(uart);
    const uint8_t pattern[2] = {0xA5u, 0x3Cu};
    for (const uint8_t byte : pattern) {
        wr(uart, logic::kRbrThr, byte);
        uint32_t polls = 0u;
        while ((rd(uart, logic::kLsr) & logic::kLsrDataReady) == 0u) {
            if (++polls >= kLoopbackPolls) return false;
        }
        if (rd(uart, logic::kRbrThr) != byte) return false;
    }
    return true;
}

// `out_uart->base` and `stride` are already set (memory-mapped windows) or zero.
sat_result_t open_common(sat_uart_t* out_uart, const sat_uart_ops_t* ops, const sat_uart_config_t* config) {
    if (out_uart == nullptr || ops == nullptr || ops->read == nullptr || ops->write == nullptr ||
        config == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    const uint32_t divisor = logic::divisor(config->clock_hz, config->baud);
    const uint8_t format = logic::line_control(config->data_bits, config->stop_bits, config->parity);
    if (divisor == 0u || format == 0xFFu) return SAT_ERR_INVALID_ARG;

    sat_uart_t uart = {};
    uart.ops = *ops;
    uart.base = out_uart->base;
    uart.stride = out_uart->stride;
    // Interrupts off and the word format set first: the loopback test needs a defined line.
    wr(&uart, logic::kIerDlm, 0u);
    wr(&uart, logic::kLcr, static_cast<uint8_t>(logic::kLcrDlab | format));
    wr(&uart, logic::kRbrThr, static_cast<uint8_t>(divisor & 0xFFu));
    wr(&uart, logic::kIerDlm, static_cast<uint8_t>(divisor >> 8u));
    wr(&uart, logic::kLcr, format);
    wr(&uart, logic::kIirFcr, logic::kFcrEnableClear);
    uart.fifo = (rd(&uart, logic::kIirFcr) & logic::kIirFifoMask) == logic::kIirFifoMask ? 1u : 0u;

    if (!responds(&uart)) {
        wr(&uart, logic::kMcr, 0u);
        return SAT_ERR_NOT_CONNECTED;
    }
    drain(&uart);
    wr(&uart, logic::kMcr, logic::kMcrDtrRts);
    uart.opened = 1u;
    *out_uart = uart;
    return SAT_OK;
}

}  // namespace

extern "C" sat_result_t sat_uart_open_ops(sat_uart_t* out_uart, const sat_uart_ops_t* ops,
                                          const sat_uart_config_t* config) {
    if (out_uart == nullptr) return SAT_ERR_INVALID_ARG;
    *out_uart = {};
    return open_common(out_uart, ops, config);
}

extern "C" sat_result_t sat_uart_open_mmio(sat_uart_t* out_uart, uintptr_t base, uint8_t stride,
                                           const sat_uart_config_t* config) {
    if (out_uart == nullptr || (stride != 1u && stride != 2u && stride != 4u)) return SAT_ERR_INVALID_ARG;
    *out_uart = {};
    out_uart->base = base;
    out_uart->stride = stride;
    const sat_uart_ops_t ops = {mmio_read, mmio_write, out_uart};
    return open_common(out_uart, &ops, config);
}

extern "C" sat_result_t sat_uart_write(sat_uart_t* uart, const void* data, uint32_t length, uint32_t poll_limit,
                                       uint32_t* out_written) {
    if (out_written != nullptr) *out_written = 0u;
    if (uart == nullptr || uart->opened == 0u || (data == nullptr && length != 0u)) return SAT_ERR_INVALID_ARG;
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    for (uint32_t i = 0u; i < length; ++i) {
        uint32_t polls = 0u;
        while ((rd(uart, logic::kLsr) & logic::kLsrThrEmpty) == 0u) {
            if (++polls >= poll_limit) return SAT_ERR_TIMEOUT;
        }
        wr(uart, logic::kRbrThr, bytes[i]);
        ++uart->bytes_sent;
        if (out_written != nullptr) *out_written = i + 1u;
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_uart_read(sat_uart_t* uart, void* out, uint32_t capacity, uint32_t* out_read) {
    if (out_read != nullptr) *out_read = 0u;
    if (uart == nullptr || uart->opened == 0u || (out == nullptr && capacity != 0u)) return SAT_ERR_INVALID_ARG;
    uint8_t* bytes = static_cast<uint8_t*>(out);
    uint32_t count = 0u;
    while (count < capacity) {
        const uint8_t lsr = rd(uart, logic::kLsr);
        count_errors(uart, lsr);
        if ((lsr & logic::kLsrDataReady) == 0u) break;
        bytes[count++] = rd(uart, logic::kRbrThr);
        ++uart->bytes_received;
    }
    if (out_read != nullptr) *out_read = count;
    return SAT_OK;
}

extern "C" uint8_t sat_uart_rx_ready(sat_uart_t* uart) {
    if (uart == nullptr || uart->opened == 0u) return 0u;
    return (rd(uart, logic::kLsr) & logic::kLsrDataReady) != 0u ? 1u : 0u;
}

extern "C" sat_result_t sat_uart_close(sat_uart_t* uart) {
    if (uart == nullptr || uart->opened == 0u) return SAT_ERR_INVALID_ARG;
    wr(uart, logic::kIerDlm, 0u);
    wr(uart, logic::kMcr, 0u);
    uart->opened = 0u;
    return SAT_OK;
}
