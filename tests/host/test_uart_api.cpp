#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <vector>

#include "saturn/uart.h"
#include "src/hal/comm/uart_logic.hpp"

using namespace saturn::hal::comm::uart_logic;

// A model of the 16550: divisor latch behind DLAB, scratch register, a receive queue, internal
// loopback and the line status bits. Transmission completes at once unless it is made to stall.
struct Model {
    bool present = true;              // false: every read is 0xFF and writes vanish (an empty bus)
    bool has_scratch = true;
    bool fifo = true;
    bool tx_stuck = false;            // THRE never comes
    uint8_t ier = 0, lcr = 0, mcr = 0, scr = 0, fcr = 0;
    uint8_t dll = 0, dlm = 0;
    std::deque<uint8_t> rx;
    uint8_t pending_errors = 0;       // LSR error bits, cleared by reading LSR
    std::vector<uint8_t> sent;
    uint32_t lsr_reads = 0;
};

static uint8_t model_read(void* context, uint8_t reg) {
    Model& m = *static_cast<Model*>(context);
    if (!m.present) return 0xFFu;
    const bool dlab = (m.lcr & kLcrDlab) != 0u;
    switch (reg) {
    case kRbrThr:
        if (dlab) return m.dll;
        if (m.rx.empty()) return 0u;
        {
            const uint8_t byte = m.rx.front();
            m.rx.pop_front();
            return byte;
        }
    case kIerDlm: return dlab ? m.dlm : m.ier;
    case kIirFcr: return (m.fcr & 1u) != 0u && m.fifo ? 0xC1u : 0x01u;
    case kLcr: return m.lcr;
    case kMcr: return m.mcr;
    case kLsr: {
        ++m.lsr_reads;
        uint8_t lsr = m.tx_stuck ? 0u : kLsrThrEmpty;
        if (!m.rx.empty()) lsr |= kLsrDataReady;
        lsr |= m.pending_errors;
        m.pending_errors = 0u;
        return lsr;
    }
    case kMsr: return 0u;
    case kScr: return m.has_scratch ? m.scr : 0xFFu;
    }
    return 0u;
}

static void model_write(void* context, uint8_t reg, uint8_t value) {
    Model& m = *static_cast<Model*>(context);
    if (!m.present) return;
    const bool dlab = (m.lcr & kLcrDlab) != 0u;
    switch (reg) {
    case kRbrThr:
        if (dlab) {
            m.dll = value;
        } else if ((m.mcr & kMcrLoopback) != 0u) {
            m.rx.push_back(value);
        } else {
            m.sent.push_back(value);
        }
        break;
    case kIerDlm:
        if (dlab) m.dlm = value;
        else m.ier = value;
        break;
    case kIirFcr:
        m.fcr = value;
        if ((value & 0x02u) != 0u) m.rx.clear();
        break;
    case kLcr: m.lcr = value; break;
    case kMcr: m.mcr = value; break;
    case kScr:
        if (m.has_scratch) m.scr = value;
        break;
    }
}

static sat_uart_ops_t ops_for(Model& m) {
    return {model_read, model_write, &m};
}

int main() {
    // arithmetic
    assert(divisor(1843200u, 115200u) == 1u);
    assert(divisor(1843200u, 9600u) == 12u);
    assert(divisor(1843200u, 57600u) == 2u);
    assert(divisor(1843200u, 0u) == 0u && divisor(0u, 9600u) == 0u);
    assert(divisor(1843200u, 1u) == 0u);            // 115200 does not fit 16 bits
    assert(divisor(1000000u, 1000000u) == 0u);      // rounds to 0: too fast
    assert(line_control(8u, 1u, 0u) == 0x03u);
    assert(line_control(7u, 2u, 1u) == (0x02u | 0x04u | 0x08u));
    assert(line_control(8u, 1u, 2u) == (0x03u | 0x18u));
    assert(line_control(4u, 1u, 0u) == 0xFFu && line_control(8u, 3u, 0u) == 0xFFu && line_control(8u, 1u, 3u) == 0xFFu);

    const sat_uart_config_t config = {1843200u, 9600u, 8u, 1u, 0u, 0u};

    // opening programs the chip and finds the FIFO
    Model chip;
    sat_uart_t uart = {};
    sat_uart_ops_t ops = ops_for(chip);
    assert(sat_uart_open_ops(&uart, &ops, &config) == SAT_OK);
    assert(uart.opened == 1u && uart.fifo == 1u);
    assert(chip.dll == 12u && chip.dlm == 0u);
    assert(chip.lcr == 0x03u);                       // 8N1, DLAB off again
    assert(chip.ier == 0u);
    assert((chip.fcr & 0x07u) == 0x07u);
    assert(chip.mcr == kMcrDtrRts);                  // out of loopback, lines up
    assert(chip.rx.empty() && chip.sent.empty());    // the self-test left nothing behind

    // a plain 16450 (no FIFO) still opens
    Model old_chip;
    old_chip.fifo = false;
    ops = ops_for(old_chip);
    assert(sat_uart_open_ops(&uart, &ops, &config) == SAT_OK && uart.fifo == 0u);

    // an empty bus, or a chip whose scratch register is missing, is not a UART
    Model empty;
    empty.present = false;
    ops = ops_for(empty);
    assert(sat_uart_open_ops(&uart, &ops, &config) == SAT_ERR_NOT_CONNECTED && uart.opened == 0u);
    Model no_scratch;
    no_scratch.has_scratch = false;
    ops = ops_for(no_scratch);
    assert(sat_uart_open_ops(&uart, &ops, &config) == SAT_ERR_NOT_CONNECTED);
    assert(no_scratch.mcr == 0u);                    // left quiet

    // bad configuration
    ops = ops_for(chip);
    sat_uart_config_t bad = config;
    bad.baud = 0u;
    assert(sat_uart_open_ops(&uart, &ops, &bad) == SAT_ERR_INVALID_ARG);
    bad = config;
    bad.data_bits = 9u;
    assert(sat_uart_open_ops(&uart, &ops, &bad) == SAT_ERR_INVALID_ARG);
    assert(sat_uart_open_ops(nullptr, &ops, &config) == SAT_ERR_INVALID_ARG);
    assert(sat_uart_open_ops(&uart, nullptr, &config) == SAT_ERR_INVALID_ARG);
    assert(sat_uart_open_ops(&uart, &ops, nullptr) == SAT_ERR_INVALID_ARG);
    assert(sat_uart_open_mmio(&uart, 0x1000u, 3u, &config) == SAT_ERR_INVALID_ARG);   // stride must be 1, 2 or 4

    // 115200 baud with a 16 bit divisor above 255
    sat_uart_config_t slow = {1843200u, 300u, 7u, 2u, 2u, 0u};
    Model slow_chip;
    ops = ops_for(slow_chip);
    assert(sat_uart_open_ops(&uart, &ops, &slow) == SAT_OK);
    assert(slow_chip.dll == (384u & 0xFFu) && slow_chip.dlm == (384u >> 8u));
    assert(slow_chip.lcr == line_control(7u, 2u, 2u));

    // sending
    ops = ops_for(chip);
    assert(sat_uart_open_ops(&uart, &ops, &config) == SAT_OK);
    uint32_t written = 99u;
    const uint8_t hello[5] = {'A', 'T', 'Z', '\r', 0xFFu};
    assert(sat_uart_write(&uart, hello, sizeof(hello), 100u, &written) == SAT_OK && written == 5u);
    assert(chip.sent.size() == 5u && std::memcmp(chip.sent.data(), hello, 5u) == 0 && uart.bytes_sent == 5u);
    assert(sat_uart_write(&uart, nullptr, 0u, 100u, &written) == SAT_OK && written == 0u);
    assert(sat_uart_write(&uart, nullptr, 3u, 100u, &written) == SAT_ERR_INVALID_ARG);
    chip.tx_stuck = true;
    assert(sat_uart_write(&uart, hello, 3u, 10u, &written) == SAT_ERR_TIMEOUT && written == 0u);
    chip.tx_stuck = false;
    assert(chip.sent.size() == 5u);

    // receiving
    for (uint8_t byte : {1u, 2u, 3u, 4u, 5u, 6u}) chip.rx.push_back(static_cast<uint8_t>(byte));
    assert(sat_uart_rx_ready(&uart) == 1u);
    uint8_t buffer[4] = {};
    uint32_t got = 99u;
    assert(sat_uart_read(&uart, buffer, sizeof(buffer), &got) == SAT_OK && got == 4u);
    assert(buffer[0] == 1u && buffer[3] == 4u);
    assert(sat_uart_read(&uart, buffer, sizeof(buffer), &got) == SAT_OK && got == 2u && buffer[1] == 6u);
    assert(sat_uart_read(&uart, buffer, sizeof(buffer), &got) == SAT_OK && got == 0u);   // nothing waiting: no wait
    assert(sat_uart_rx_ready(&uart) == 0u && uart.bytes_received == 6u);

    // line errors are counted, once each
    chip.pending_errors = kLsrOverrun | kLsrFraming;
    assert(sat_uart_read(&uart, buffer, sizeof(buffer), &got) == SAT_OK && got == 0u);
    chip.pending_errors = kLsrParity | kLsrBreak;
    chip.rx.push_back(7u);
    assert(sat_uart_read(&uart, buffer, sizeof(buffer), &got) == SAT_OK && got == 1u);
    assert(uart.overruns == 1u && uart.framing_errors == 1u && uart.parity_errors == 1u && uart.breaks == 1u);

    // closing drops the lines and the handle refuses further use
    assert(sat_uart_close(&uart) == SAT_OK);
    assert(chip.mcr == 0u && chip.ier == 0u);
    assert(sat_uart_write(&uart, hello, 1u, 10u, &written) == SAT_ERR_INVALID_ARG);
    assert(sat_uart_read(&uart, buffer, 1u, &got) == SAT_ERR_INVALID_ARG);
    assert(sat_uart_rx_ready(&uart) == 0u && sat_uart_close(&uart) == SAT_ERR_INVALID_ARG);

    std::puts("PASS: test_uart_api.cpp");
    return 0;
}
