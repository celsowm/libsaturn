#ifndef SATURN_MPEG_H
#define SATURN_MPEG_H

#include <stdint.h>

#include "saturn/cd_block.h"
#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The Video CD (MPEG) card: detection and start-up only.
 *
 * The card is driven through CD Block commands. sat_mpeg_probe() reads the CD
 * Block's hardware information (which reports the MPEG version, 0 without a
 * card) and whether the MPEG device is authenticated. sat_mpeg_start()
 * authenticates the card and runs MPEG Init. Decoding, stream connections and
 * display control are not implemented: they need the card.
 *
 * UNVERIFIED: neither Ymir nor Mednafen emulates an MPEG card, so the present
 * path is checked against the command layouts of the Ymir core on the host
 * only; on both emulators the probe reports no card (MPEG version 0) and
 * sat_mpeg_start() returns SAT_ERR_NOT_CONNECTED without sending anything. It
 * is on docs/HARDWARE_ACCEPTANCE_CHECKLIST.md for a real Saturn with the card. */

typedef struct sat_mpeg_info {
    uint8_t present;            /* MPEG version is not 0 */
    uint8_t mpeg_version;
    uint8_t hardware_flags;
    uint8_t hardware_version;
    uint8_t drive_version;
    uint8_t drive_revision;
    uint8_t authenticated;      /* Is Device Authenticated (MPEG) reports done */
    uint8_t authentication_status; /* the raw value */
} sat_mpeg_info_t;

/* Sends Get Hardware Info and Is Device Authenticated. Changes nothing on the
 * card. `block` must be an initialized CD Block. */
sat_result_t sat_mpeg_probe(sat_cd_block_t* block, sat_mpeg_info_t* out_info);

/* SAT_ERR_NOT_CONNECTED without a card; otherwise Authenticate Device (MPEG),
 * a wait for the authentication to complete (SAT_ERR_TIMEOUT if it does not),
 * and MPEG Init (SAT_ERR_VERIFY_FAILED if the block still reports the card
 * unauthenticated). */
sat_result_t sat_mpeg_start(sat_cd_block_t* block);

#ifdef __cplusplus
}
#endif

#endif
