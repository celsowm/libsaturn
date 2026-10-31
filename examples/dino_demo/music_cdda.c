#include "music_cdda.h"
#include "saturn/video.h"

#if DINO_MUSIC_CDDA

/* SCSP manual 4.2.7: EXTS0/1 return through slot 16/17 low-byte EFSDL/EFPAN.
 * Preserve the unrelated high-byte direct-send controls. CD-DA is already
 * stereo 44.1 kHz PCM; neither SH-2 nor 68000 decodes or resamples it. */
#define EXTS_LEFT (*(volatile uint16_t*)0x25B00216u)
#define EXTS_RIGHT (*(volatile uint16_t*)0x25B00236u)

static sat_result_t wait_flag(uint16_t flag) {
    for (uint32_t field = 0; field < 120u; ++field) {
        if ((*(volatile uint16_t*)SAT_CD_BLOCK_HIRQ & flag) != 0u) return SAT_OK;
        sat_result_t st = sat_wait_vblank();
        if (st != SAT_OK) return st;
    }
    return SAT_ERR_TIMEOUT;
}

static sat_result_t audio_range(sat_cd_block_t* block, uint32_t* start, uint32_t* length) {
    const uint16_t toc_command[4] = {0x0200u, 0u, 0u, 0u};
    const uint16_t end_command[4] = {0x0600u, 0u, 0u, 0u};
    uint16_t response[4];
    /* A TOC is 102 big-endian longwords: 99 tracks, first, last, lead-out. */
    *(volatile uint16_t*)SAT_CD_BLOCK_HIRQ = (uint16_t)~SAT_CD_BLOCK_HIRQ_DRDY;
    sat_result_t st = sat_cd_block_command(block, toc_command, response);
    if (st != SAT_OK) return st;
    if (response[1] != 204u) {
        (void)sat_cd_block_command(block, end_command, response);
        return SAT_ERR_IO;
    }
    st = wait_flag(SAT_CD_BLOCK_HIRQ_DRDY);
    if (st != SAT_OK) return st;
    uint32_t track = 0u, lead_out = 0u, last = 0u;
    for (uint32_t index = 0; index < 102u; ++index) {
        const uint32_t hi = *(volatile uint16_t*)SAT_CD_BLOCK_DTR;
        const uint32_t lo = *(volatile uint16_t*)SAT_CD_BLOCK_DTR;
        const uint32_t entry = (hi << 16u) | lo;
        if (index == 1u) track = entry;
        if (index == 100u) last = entry;
        if (index == 101u) lead_out = entry;
    }
    st = sat_cd_block_command(block, end_command, response);
    if (st != SAT_OK) return st;
    if (response[1] != 204u) return SAT_ERR_IO;
    *start = track & 0x00FFFFFFu;
    const uint32_t end = lead_out & 0x00FFFFFFu;
    if ((track >> 24u) != 0x01u || ((last >> 16u) & 255u) != 2u || end <= *start)
        return SAT_ERR_INVALID_ARG;
    *length = end - *start;
    return SAT_OK;
}

sat_result_t dino_music_cdda_start(sat_cd_block_t* block) {
    uint32_t start, length;
    sat_result_t st = audio_range(block, &start, &length);
    if (st != SAT_OK) return st;
    /* Explicit FAD start and length also define the final-track boundary
     * without depending on a following track/index report at lead-out. */
    const uint16_t command[4] = {
        (uint16_t)(0x1080u | (start >> 16u)), (uint16_t)start,
        (uint16_t)(0x0F80u | (length >> 16u)), (uint16_t)length
    };
    uint16_t response[4];
    st = sat_cd_block_command(block, command, response);
    if (st != SAT_OK) return st;
    if ((response[0] >> 8u) == 0xFFu) return SAT_ERR_IO;
    /* Full-level left/right separation; no PCM voices are active. */
    EXTS_LEFT = (uint16_t)((EXTS_LEFT & 0xFF00u) | 0x00FFu);
    EXTS_RIGHT = (uint16_t)((EXTS_RIGHT & 0xFF00u) | 0x00EFu);
    return SAT_OK;
}

sat_result_t dino_music_cdda_status(void) {
    /* Read the periodic response without issuing polling commands. No data
     * reads may interrupt the drive while the reference soundtrack plays. */
    const uint8_t status = (uint8_t)(*(volatile uint16_t*)SAT_CD_BLOCK_CR1 >> 8u);
    const uint8_t state = status & 0x0Fu;
    return status == 0xFFu || state == 6u || state == 7u || state == 9u || state == 10u
        ? SAT_ERR_IO : SAT_OK;
}

#endif
