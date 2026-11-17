
#ifndef DMA_H
#define DMA_H

#include <stdint.h>
#include "config.h"

#define DMA_OK 0
#define DMA_ERR 1

uint8_t dma_init(void);
/* Copia bloqueante de palabras alineadas, sin solapamiento. El llamador
 * debe garantizar que origen y destino sean accesibles para el DMA. */
uint8_t dma_copy(const void *src, void *dst, uint32_t n_words);
uint8_t dma_is_done(void);

#endif

