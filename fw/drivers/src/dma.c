#include "dma.h"


uint8_t dma_init(void) {
    REG_DMA_START = 0;
    return DMA_OK;
}

uint8_t dma_copy(const void *src, void *dst, uint32_t n_words) {

    uintptr_t origen = (uintptr_t)src;
    uintptr_t destino = (uintptr_t)dst;

    /* Las direcciones deben ser no nulas y estar alineadas a 4 bytes. */
    if (origen == 0 || destino == 0 || (origen & 3u) || (destino & 3u)) {
        return DMA_ERR;
    }

    /* No iniciar una copia vacía ni permitir que el tamaño desborde. */
    if (n_words == 0 || n_words > UINTPTR_MAX / 4u) {
        return DMA_ERR;
    }

    uintptr_t bytes = (uintptr_t)n_words * 4u;

    /* Comprobar que los finales de los rangos no se desborden. */
    if (origen > UINTPTR_MAX - bytes || destino > UINTPTR_MAX - bytes) {
        return DMA_ERR;
    }

    /* Los rangos no deben compartir ninguna dirección. */
    if (origen < destino + bytes && destino < origen + bytes) {
        return DMA_ERR;
    }
    
    REG_DMA_SOURCE_ADDR = origen;
    REG_DMA_DEST_ADDR = destino;
    REG_DMA_SIZE = n_words;

    REG_DMA_START = 0;
    REG_DMA_START = 1;
    
    while(REG_DMA_DONE == 0) {}


    return DMA_OK;
}

uint8_t dma_is_done(void) {
    return REG_DMA_DONE;
}
