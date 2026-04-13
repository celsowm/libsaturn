#ifndef SATURN_CORE_H
#define SATURN_CORE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Fixed-point math type                                               */
/* ------------------------------------------------------------------ */
typedef int32_t sat_fx16_t;

#define SAT_FX16_ONE ((sat_fx16_t)0x00010000)

/* ------------------------------------------------------------------ */
/* Result codes                                                        */
/* ------------------------------------------------------------------ */
typedef enum sat_result {
    SAT_OK = 0,
    SAT_ERR_INVALID_ARG = -1,
    SAT_ERR_NOT_INITIALIZED = -2,
    SAT_ERR_CAPACITY = -3,
    SAT_ERR_UNSUPPORTED = -4
} sat_result_t;

/* ------------------------------------------------------------------ */
/* Video config (needed early by core init)                            */
/* ------------------------------------------------------------------ */
typedef struct sat_video_config {
    uint16_t width;
    uint16_t height;
    uint8_t ntsc;
    uint8_t reserved;
} sat_video_config_t;

/* ------------------------------------------------------------------ */
/* Core lifecycle                                                      */
/* ------------------------------------------------------------------ */
sat_result_t sat_init(const sat_video_config_t* config);
sat_result_t sat_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_CORE_H */
