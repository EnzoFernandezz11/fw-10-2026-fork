#ifndef FRAME_PARSER_H
#define FRAME_PARSER_H

#include <stdint.h>

#define FP_MIN_LENGTH      2u
#define FP_MAX_LENGTH      1032u
#define FP_SILENCE_CLOCKS  40000u

typedef enum {
    FP_WAIT_HDR0 = 0,
    FP_WAIT_HDR1,
    FP_WAIT_LEN_LO,
    FP_WAIT_LEN_HI,
    FP_RECV_BODY,
    FP_WAIT_FTR0,
    FP_WAIT_FTR1
} fp_state_t;

typedef enum {
    FP_RES_IN_PROGRESS = 0,
    FP_RES_FRAME_READY,
    FP_RES_FRAME_ERROR
} fp_result_kind_t;

typedef struct {
    fp_result_kind_t kind;
    const uint8_t   *body;          /* OPCODE + PAYLOAD + CRC (solo en FRAME_READY) */
    uint16_t         length;        /* LENGTH recibido (solo en FRAME_READY) */
    uint8_t          footer_ok;     /* 1 si FOOTER == 0xC3 0x3C (solo en FRAME_READY) */
    uint8_t          opcode;        /* opcode para reportar en FRAME_ERROR (0 si no llego) */
    uint8_t          error_status;  /* STATUS_ERROR_* (solo en FRAME_ERROR) */
} fp_result_t;

typedef struct {
    fp_state_t state;
    uint16_t   length;
    uint16_t   idx;
    uint8_t    body[FP_MAX_LENGTH];
    uint8_t    ftr0_ok;
    uint8_t    prev_was_a5;
    uint32_t   last_rx_clk;
} frame_parser_t;

void        frame_parser_init(frame_parser_t *p);
fp_result_t frame_parser_feed(frame_parser_t *p, uint8_t byte);
void        frame_parser_check_timeout(frame_parser_t *p);

#endif /* FRAME_PARSER_H */
