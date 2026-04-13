#ifndef SATURN_INPUT_H
#define SATURN_INPUT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Digital pad bit masks                                               */
/* ------------------------------------------------------------------ */
#define SAT_PAD_UP    ((uint16_t)(1u << 12))
#define SAT_PAD_DOWN  ((uint16_t)(1u << 13))
#define SAT_PAD_LEFT  ((uint16_t)(1u << 14))
#define SAT_PAD_RIGHT ((uint16_t)(1u << 15))
#define SAT_PAD_START ((uint16_t)(1u << 3))
#define SAT_PAD_A     ((uint16_t)(1u << 2))
#define SAT_PAD_B     ((uint16_t)(1u << 4))
#define SAT_PAD_C     ((uint16_t)(1u << 5))
#define SAT_PAD_X     ((uint16_t)(1u << 6))
#define SAT_PAD_Y     ((uint16_t)(1u << 7))
#define SAT_PAD_Z     ((uint16_t)(1u << 8))
#define SAT_PAD_L     ((uint16_t)(1u << 9))
#define SAT_PAD_R     ((uint16_t)(1u << 10))

/* ------------------------------------------------------------------ */
/* Pad state                                                           */
/* ------------------------------------------------------------------ */
typedef struct sat_pad_state {
    uint16_t held;
    uint16_t pressed;
    uint16_t released;
} sat_pad_state_t;

/* ------------------------------------------------------------------ */
/* Input polling                                                       */
/* ------------------------------------------------------------------ */
sat_result_t sat_pad_poll(sat_pad_state_t* out_state);
uint16_t sat_pad_held(void);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_INPUT_H */
