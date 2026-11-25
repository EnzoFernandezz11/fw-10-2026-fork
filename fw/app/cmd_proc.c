#include "cmd_proc.h"
#include "cmd_protocol_defs.h"
#include "config.h"
#include "adc.h"
#include "dma.h"
#include "led.h"

#define CMD_MAX_WORDS  256u

typedef void (*cmd_handler_t)(const cmd_request_t *req, cmd_response_t *resp);

typedef struct {
    uint8_t       opcode;
    cmd_handler_t handler;
} cmd_entry_t;

/* Reconstruccion little-endian byte a byte para evitar TRAP por desalineacion en RV32I */
static uint16_t read_u16_le(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_u32_le(const uint8_t *p)
{
    return  (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

static void write_u32_le(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static int in_region(uint32_t addr, uint32_t size, uint32_t start, uint32_t rsize)
{
    return addr >= start && (addr - start) < rsize && size <= (rsize - (addr - start));
}

/* Valida alineacion, rango completo dentro de una sola region y proteccion de REG_MCU_RESET */
static int mem_range_valid(uint32_t addr, uint32_t n_words, int is_write)
{
    if ((addr & 3u) != 0 || n_words == 0 || n_words > (0xFFFFFFFFu / 4u)) {
        return 0;
    }

    uint32_t size = n_words * 4u;
    int valid_region = in_region(addr, size, ADDR_START_INST, ADDR_SIZE_INST)
                    || in_region(addr, size, ADDR_START_DATA, ADDR_SIZE_DATA)
                    || in_region(addr, size, ADDR_START_REG_FILE, ADDR_SIZE_REG_FILE);
    if (!valid_region) {
        return 0;
    }

    if (is_write) {
        uint32_t rst_addr = (uint32_t)(uintptr_t)&REG_MCU_RESET;
        if (rst_addr >= addr && (rst_addr - addr) < size) {
            return 0;
        }
    }

    return 1;
}

static void cmd_handle_adc(const cmd_request_t *req, cmd_response_t *resp)
{
    static uint32_t adc_buf[CMD_MAX_WORDS];

    if (req->payload_len != 7u) {
        resp->status = STATUS_ERROR_LENGTH;
        return;
    }

    uint32_t clk_div     = read_u32_le(&req->payload[0]);
    uint8_t  mode        = req->payload[4];
    uint16_t num_samples = read_u16_le(&req->payload[5]);

    if (clk_div == 0 || mode != 0 || num_samples < 1u || num_samples > CMD_MAX_WORDS) {
        resp->status = STATUS_ERROR_INVALID_PARAM;
        return;
    }

    if (adc_init(clk_div) != ADC_OK) {
        resp->status = STATUS_ERROR_HW;
        return;
    }

    uint32_t got = adc_read(adc_buf, num_samples);
    if (got != num_samples) {
        resp->status = STATUS_ERROR_HW;
        return;
    }

    for (uint16_t i = 0; i < num_samples; i++) {
        write_u32_le(&resp->data[i * 4u], adc_buf[i]);
    }
    resp->data_len = (uint16_t)(num_samples * 4u);
    resp->status   = STATUS_OK;
}

static void cmd_handle_dma(const cmd_request_t *req, cmd_response_t *resp)
{
    if (req->payload_len != 12u) {
        resp->status = STATUS_ERROR_LENGTH;
        return;
    }

    uint32_t src_addr   = read_u32_le(&req->payload[0]);
    uint32_t dst_addr   = read_u32_le(&req->payload[4]);
    uint32_t size_words = read_u32_le(&req->payload[8]);

    if (!mem_range_valid(src_addr, size_words, 0) || !mem_range_valid(dst_addr, size_words, 1)) {
        resp->status = STATUS_ERROR_INVALID_PARAM;
        return;
    }

    if (dma_copy((const void *)(uintptr_t)src_addr, (void *)(uintptr_t)dst_addr, size_words) != DMA_OK) {
        resp->status = STATUS_ERROR_INVALID_PARAM;
        return;
    }

    resp->status = STATUS_OK;
}

static void cmd_handle_mem_read(const cmd_request_t *req, cmd_response_t *resp)
{
    if (req->payload_len != 6u) {
        resp->status = STATUS_ERROR_LENGTH;
        return;
    }

    uint32_t addr  = read_u32_le(&req->payload[0]);
    uint16_t words = read_u16_le(&req->payload[4]);

    if (words < 1u || words > CMD_MAX_WORDS || !mem_range_valid(addr, words, 0)) {
        resp->status = STATUS_ERROR_INVALID_PARAM;
        return;
    }

    volatile const uint32_t *src = (volatile const uint32_t *)(uintptr_t)addr;
    for (uint16_t i = 0; i < words; i++) {
        write_u32_le(&resp->data[i * 4u], src[i]);
    }
    resp->data_len = (uint16_t)(words * 4u);
    resp->status   = STATUS_OK;
}

static void cmd_handle_mem_write(const cmd_request_t *req, cmd_response_t *resp)
{
    if (req->payload_len < 6u) {
        resp->status = STATUS_ERROR_LENGTH;
        return;
    }

    uint32_t addr  = read_u32_le(&req->payload[0]);
    uint16_t words = read_u16_le(&req->payload[4]);

    if (words < 1u || words > CMD_MAX_WORDS) {
        resp->status = STATUS_ERROR_INVALID_PARAM;
        return;
    }

    if (req->payload_len != (uint16_t)(6u + words * 4u)) {
        resp->status = STATUS_ERROR_LENGTH;
        return;
    }

    if (!mem_range_valid(addr, words, 1)) {
        resp->status = STATUS_ERROR_INVALID_PARAM;
        return;
    }

    volatile uint32_t *dst = (volatile uint32_t *)(uintptr_t)addr;
    for (uint16_t i = 0; i < words; i++) {
        dst[i] = read_u32_le(&req->payload[6u + i * 4u]);
    }
    resp->status = STATUS_OK;
}

static void cmd_handle_led(const cmd_request_t *req, cmd_response_t *resp)
{
    if (req->payload_len < 1u) {
        resp->status = STATUS_ERROR_LENGTH;
        return;
    }

    uint8_t op = req->payload[0];
    if (op == LED_OP_SET) {
        if (req->payload_len != 2u) {
            resp->status = STATUS_ERROR_LENGTH;
            return;
        }
        led_write(req->payload[1]);
        resp->status = STATUS_OK;
    } else if (op == LED_OP_GET) {
        if (req->payload_len != 1u) {
            resp->status = STATUS_ERROR_LENGTH;
            return;
        }
        resp->data[0]  = led_get_mask();
        resp->data_len = 1u;
        resp->status   = STATUS_OK;
    } else {
        resp->status = STATUS_ERROR_INVALID_PARAM;
    }
}

static const cmd_entry_t g_cmd_table[] = {
    { OP_ADC,       cmd_handle_adc       },
    { OP_DMA,       cmd_handle_dma       },
    { OP_MEM_READ,  cmd_handle_mem_read  },
    { OP_MEM_WRITE, cmd_handle_mem_write },
    { OP_LED,       cmd_handle_led       },
};

void cmd_proc_execute(const cmd_request_t *req, cmd_response_t *resp)
{
    resp->data_len = 0;
    for (uint32_t i = 0; i < (sizeof(g_cmd_table) / sizeof(g_cmd_table[0])); i++) {
        if (g_cmd_table[i].opcode == req->opcode) {
            g_cmd_table[i].handler(req, resp);
            return;
        }
    }
    resp->status = STATUS_ERROR_UNKNOWN_OPCODE;
}
