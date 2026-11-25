#include "frame_parser.h"
#include "cmd_protocol_defs.h"
#include "clock.h"

static void fp_reset(frame_parser_t *p, fp_state_t next_state)
{
    p->state       = next_state;
    p->length      = 0;
    p->idx         = 0;
    p->ftr0_ok     = 0;
    p->prev_was_a5 = 0;
}

void frame_parser_init(frame_parser_t *p)
{
    fp_reset(p, FP_WAIT_HDR0);
    p->last_rx_clk = clock_cycles();
}

void frame_parser_check_timeout(frame_parser_t *p)
{
    if (p->state != FP_WAIT_HDR0 && clock_elapsed(p->last_rx_clk) > FP_SILENCE_CLOCKS) {
        fp_reset(p, FP_WAIT_HDR0);
    }
}

fp_result_t frame_parser_feed(frame_parser_t *p, uint8_t byte)
{
    fp_result_t res = { .kind = FP_RES_IN_PROGRESS };

    p->last_rx_clk = clock_cycles();

    /* Nuevo HEADER (0xA5 0x5A) interrumpiendo una trama activa */
    if (p->state >= FP_WAIT_LEN_LO && p->prev_was_a5 && byte == PKT_HEADER_1) {
        uint8_t interrupted_op = 0;
        /* Si body[0] ya llego antes del 0xA5, ese es el opcode de la trama cortada */
        if (p->state > FP_RECV_BODY || (p->state == FP_RECV_BODY && p->idx >= 2u)) {
            interrupted_op = p->body[0];
        }
        fp_reset(p, FP_WAIT_LEN_LO);
        res.kind         = FP_RES_FRAME_ERROR;
        res.opcode       = interrupted_op;
        res.error_status = STATUS_ERROR_LENGTH;
        return res;
    }

    switch (p->state) {
    case FP_WAIT_HDR0:
        if (byte == PKT_HEADER_0) {
            p->state = FP_WAIT_HDR1;
        }
        break;

    case FP_WAIT_HDR1:
        if (byte == PKT_HEADER_1) {
            fp_reset(p, FP_WAIT_LEN_LO);
        } else if (byte != PKT_HEADER_0) {
            p->state = FP_WAIT_HDR0;
        }
        break;

    case FP_WAIT_LEN_LO:
        p->length      = byte;
        p->prev_was_a5 = (byte == PKT_HEADER_0);
        p->state       = FP_WAIT_LEN_HI;
        break;

    case FP_WAIT_LEN_HI:
        p->length |= ((uint16_t)byte << 8);
        if (p->length < FP_MIN_LENGTH || p->length > FP_MAX_LENGTH) {
            /* Si byte == 0xA5, conservarlo como posible inicio de nuevo HEADER */
            fp_reset(p, (byte == PKT_HEADER_0) ? FP_WAIT_HDR1 : FP_WAIT_HDR0);
            res.kind         = FP_RES_FRAME_ERROR;
            res.opcode       = 0;
            res.error_status = STATUS_ERROR_LENGTH;
        } else {
            p->idx         = 0;
            p->prev_was_a5 = (byte == PKT_HEADER_0);
            p->state       = FP_RECV_BODY;
        }
        break;

    case FP_RECV_BODY:
        p->body[p->idx++] = byte;
        p->prev_was_a5    = (byte == PKT_HEADER_0);
        if (p->idx == p->length) {
            p->state = FP_WAIT_FTR0;
        }
        break;

    case FP_WAIT_FTR0:
        p->ftr0_ok     = (byte == PKT_FOOTER_0);
        p->prev_was_a5 = (byte == PKT_HEADER_0);
        p->state       = FP_WAIT_FTR1;
        break;

    case FP_WAIT_FTR1:
        res.kind      = FP_RES_FRAME_READY;
        res.body      = p->body;
        res.length    = p->length;
        res.footer_ok = (p->ftr0_ok && byte == PKT_FOOTER_1) ? 1u : 0u;
        fp_reset(p, (byte == PKT_HEADER_0) ? FP_WAIT_HDR1 : FP_WAIT_HDR0);
        break;
    }

    return res;
}
