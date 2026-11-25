
#ifndef DMA_H
#define DMA_H

#include <stdint.h>
#include "config.h"

#define DMA_OK 0
#define DMA_ERR 1

uint8_t dma_init(void);
/* Copia bloqueante de palabras alineadas, sin solapamiento. Si el rango de
 * origen o de destino sale de 0x0000_0000-0x0005_FFFF no inicia la copia
 * y devuelve DMA_ERR. */
uint8_t dma_copy(const void *src, void *dst, uint32_t n_words);
uint8_t dma_is_done(void);

#endif

