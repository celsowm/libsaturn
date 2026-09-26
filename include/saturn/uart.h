#ifndef SATURN_UART_H
#define SATURN_UART_H

#include <stddef.h>
#include <stdint.h>

#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A driver for the 16550-family UART, the chip behind the NetLink modem
 * cartridge's serial side, over any register window.
 *
 * NOT VERIFIED ON HARDWARE OR EMULATOR. There is no NetLink in Ymir or
 * Mednafen, and the repository holds no NetLink documentation, so LibSaturn
 * ships no NetLink address and no modem (AT command) layer: the caller supplies
 * where the UART registers are, and a wrong address would write to whatever is
 * there (the CD Block shares that part of the A-Bus, for one). The driver
 * itself is checked on the host against a model of the 16550. It is on
 * docs/HARDWARE_ACCEPTANCE_CHECKLIST.md for a real Saturn with the cartridge.
 *
 * Opening a UART tests it: the scratch register and an internal loopback byte
 * must work, otherwise nothing is left configured and SAT_ERR_NOT_CONNECTED
 * comes back. */

typedef struct sat_uart_ops {
    uint8_t (*read)(void* context, uint8_t reg);          /* reg 0-7 of the UART */
    void (*write)(void* context, uint8_t reg, uint8_t value);
    void* context;
} sat_uart_ops_t;

typedef struct sat_uart_config {
    uint32_t clock_hz;          /* the UART's crystal, e.g. 1843200 */
    uint32_t baud;
    uint8_t data_bits;          /* 5-8 */
    uint8_t stop_bits;          /* 1 or 2 */
    uint8_t parity;             /* 0 none, 1 odd, 2 even */
    uint8_t reserved;
} sat_uart_config_t;

typedef struct sat_uart {
    sat_uart_ops_t ops;
    uintptr_t base;             /* memory-mapped windows */
    uint8_t stride;             /* bytes between registers */
    uint8_t opened;
    uint8_t fifo;               /* 1: a 16550A FIFO answered */
    uint8_t reserved;
    uint32_t overruns;
    uint32_t parity_errors;
    uint32_t framing_errors;
    uint32_t breaks;
    uint32_t bytes_sent;
    uint32_t bytes_received;
} sat_uart_t;

/* Registers at base + reg * stride (stride 1, 2 or 4), byte accesses. `base`
 * is the address of register 0, which may be odd (registers on the low byte of
 * 16-bit or 32-bit bus words). */
sat_result_t sat_uart_open_mmio(sat_uart_t* out_uart, uintptr_t base, uint8_t stride,
                                const sat_uart_config_t* config);
/* Registers through callbacks (a UART behind another device, or a model). */
sat_result_t sat_uart_open_ops(sat_uart_t* out_uart, const sat_uart_ops_t* ops, const sat_uart_config_t* config);

/* Sends `length` bytes, waiting up to `poll_limit` status reads for each. On
 * SAT_ERR_TIMEOUT `*out_written` says how many went out. */
sat_result_t sat_uart_write(sat_uart_t* uart, const void* data, uint32_t length, uint32_t poll_limit,
                            uint32_t* out_written);
/* Copies the bytes that have arrived, up to `capacity`, without waiting. Line
 * errors (overrun, parity, framing, break) are counted in the structure. */
sat_result_t sat_uart_read(sat_uart_t* uart, void* out, uint32_t capacity, uint32_t* out_read);
/* 1 when a byte is waiting. */
uint8_t sat_uart_rx_ready(sat_uart_t* uart);
/* Interrupts off, modem lines dropped. */
sat_result_t sat_uart_close(sat_uart_t* uart);

#ifdef __cplusplus
}
#endif

#endif
