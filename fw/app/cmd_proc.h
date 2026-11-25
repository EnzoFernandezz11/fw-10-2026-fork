#ifndef CMD_PROC_H
#define CMD_PROC_H

#include <stdint.h>

#define CMD_MAX_RESP_DATA  1024u

typedef struct {
    uint8_t        opcode;
    const uint8_t *payload;
    uint16_t       payload_len;
} cmd_request_t;

typedef struct {
    uint8_t  status;
    uint8_t *data;
    uint16_t data_len;
} cmd_response_t;

void cmd_proc_execute(const cmd_request_t *req, cmd_response_t *resp);

#endif /* CMD_PROC_H */
