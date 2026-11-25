#include "uart1.h"
#include "cmd_protocol_defs.h"
#include "strutils.h"
#include "frame_parser.h"
#include "cmd_proc.h"

#define RTS_HIGH_WM  8u
#define RTS_LOW_WM   4u

static int            s_rts_on = 0;
static frame_parser_t s_parser;
static uint8_t        s_resp_data[CMD_MAX_RESP_DATA];
static uint8_t        s_tx_buf[CMD_MAX_RESP_DATA + 16u];

static void flow_control(void)
{
    uint32_t level = uart1_rx_level();
    if (s_rts_on && level >= RTS_HIGH_WM) {
        uart1_set_rts(0);
        s_rts_on = 0;
    } else if (!s_rts_on && level <= RTS_LOW_WM) {
        uart1_set_rts(1);
        s_rts_on = 1;
    }
}

static void send_response(uint8_t opcode, uint8_t status, const uint8_t *data, uint16_t data_len)
{
    if (status != STATUS_OK) {
        data_len = 0;
    }

    uint16_t body_len = (uint16_t)(2u + data_len);      /* OPCODE_RESP + STATUS + data */
    uint16_t length   = (uint16_t)(body_len + 1u);      /* + CRC */

    s_tx_buf[0] = PKT_HEADER_0;
    s_tx_buf[1] = PKT_HEADER_1;
    s_tx_buf[2] = (uint8_t)(length & 0xFFu);
    s_tx_buf[3] = (uint8_t)((length >> 8) & 0xFFu);
    s_tx_buf[4] = (uint8_t)(opcode | OP_RESPONSE_FLAG);
    s_tx_buf[5] = status;

    for (uint16_t i = 0; i < data_len; i++) {
        s_tx_buf[6u + i] = data[i];
    }

    uint32_t pos = 4u + body_len;
    s_tx_buf[pos++] = str_crc8(&s_tx_buf[4], body_len);
    s_tx_buf[pos++] = PKT_FOOTER_0;
    s_tx_buf[pos++] = PKT_FOOTER_1;

    uart1_write(s_tx_buf, pos);
}

static void handle_parser_result(const fp_result_t *res)
{
    /* Frenar al host antes de calcular CRC, ejecutar comando y enviar respuesta */
    uart1_set_rts(0);
    s_rts_on = 0;

    if (res->kind == FP_RES_FRAME_ERROR) {
        send_response(res->opcode, res->error_status, 0, 0);
        return;
    }

    uint8_t opcode   = res->body[0];
    uint8_t recv_crc = res->body[res->length - 1u];
    uint8_t calc_crc = str_crc8(res->body, res->length - 1u);

    /* Si fallan CRC y FOOTER a la vez, la consigna prioriza ERROR_CRC */
    if (calc_crc != recv_crc) {
        send_response(opcode, STATUS_ERROR_CRC, 0, 0);
        return;
    }

    if (!res->footer_ok) {
        send_response(opcode, STATUS_ERROR_LENGTH, 0, 0);
        return;
    }

    cmd_request_t req = {
        .opcode      = opcode,
        .payload     = &res->body[1],
        .payload_len = (uint16_t)(res->length - 2u),
    };
    cmd_response_t resp = {
        .status   = STATUS_OK,
        .data     = s_resp_data,
        .data_len = 0,
    };

    cmd_proc_execute(&req, &resp);
    send_response(opcode, resp.status, resp.data, resp.data_len);
}

void main(void)
{
    REG_FW_STATUS = 0;
    uart1_init(UART1_SIM_BAUD_DIV);
    frame_parser_init(&s_parser);
    uart1_set_rts(1);
    s_rts_on = 1;

    for (;;) {
        flow_control();

        uint8_t b;
        if (uart1_getc(&b)) {
            fp_result_t res = frame_parser_feed(&s_parser, b);
            if (res.kind != FP_RES_IN_PROGRESS) {
                handle_parser_result(&res);
            }
        } else {
            frame_parser_check_timeout(&s_parser);
        }
    }
}
