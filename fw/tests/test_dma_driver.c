#include "dma.h"

static uint32_t origen[] = {0x12345678u, 0xabcdef01u, 0u, 0xffffffffu};
static uint32_t destino[4];

void main(void)
{
    REG_FW_STATUS = 0;
    REG_FW_REPORT = 1;
    if (dma_init() != DMA_OK || dma_copy(origen, destino, 4) != DMA_OK)
        goto fail;

    REG_FW_REPORT = 2;
    for (uint32_t i = 0; i < 4; i++) {
        if (destino[i] != origen[i])
            goto fail;
    }

    /* Una segunda copia comprueba que START produzca otro flanco. */
    origen[0] = 42;
    REG_FW_REPORT = 3;
    if (dma_copy(origen, destino, 4) != DMA_OK || destino[0] != 42)
        goto fail;

    /* Repetir el inicio al terminar cada copia detecta DONE o START obsoletos. */
    REG_FW_REPORT = 7;
    for (uint32_t vuelta = 1; vuelta <= 32; vuelta++) {
        for (uint32_t i = 0; i < 4; i++)
            origen[i] = vuelta * 4 + i;
        if (dma_copy(origen, destino, 4) != DMA_OK || !dma_is_done())
            goto fail;
        for (uint32_t i = 0; i < 4; i++) {
            if (destino[i] != origen[i])
                goto fail;
        }
    }

    REG_FW_REPORT = 4;
    if (dma_copy((const void *)0, destino, 1) != DMA_ERR)
        goto fail;
    REG_FW_REPORT = 6;
    if (dma_copy((const uint8_t *)origen + 1, destino, 1) != DMA_ERR)
        goto fail;

    REG_FW_STATUS = 0xff;
    return;

fail:
    REG_FW_STATUS = 0xbad;
}
