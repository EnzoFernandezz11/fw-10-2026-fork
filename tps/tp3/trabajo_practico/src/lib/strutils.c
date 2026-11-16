#include "strutils.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* Inicializa el buffer como una cadena vacía. */
void strbuf_init(strbuf_t *buf)
{
    buf->data[0] = '\0';
    buf->len = 0;
}

/* Reemplaza el contenido y recorta el texto si no entra. */
void strbuf_set(strbuf_t *buf, const char *src)
{
    size_t len = strlen(src);
    if (len >= STR_MAX_LEN)
        len = STR_MAX_LEN - 1;
    memcpy(buf->data, src, len);
    buf->data[len] = '\0';
    buf->len = len;
}

/* Agrega texto hasta llenar el espacio disponible y devuelve lo agregado. */
size_t strbuf_append(strbuf_t *buf, const char *src)
{
    size_t remaining = STR_MAX_LEN - 1 - buf->len;
    size_t len = strlen(src);
    if (len > remaining)
        len = remaining;
    memcpy(buf->data + buf->len, src, len);
    buf->len += len;
    buf->data[buf->len] = '\0';
    return len;
}

/* Convierte un entero a texto y lo agrega al buffer. */
size_t strbuf_append_int(strbuf_t *buf, int32_t value)
{
    char text[12];
    if (snprintf(text, sizeof(text), "%d", (int)value) < 0)
        return 0;
    return strbuf_append(buf, text);
}

/* Devuelve la cadena almacenada para su lectura. */
const char *strbuf_cstr(const strbuf_t *buf)
{
    return buf->data;
}

/* Copia el texto sin espacios en blanco al principio ni al final. */
size_t str_trim(char *dst, const char *src, size_t max_len)
{
    if (max_len == 0)
        return 0;

    while (isspace((unsigned char)*src))
        src++;

    size_t len = strlen(src);
    while (len > 0 && isspace((unsigned char)src[len - 1]))
        len--;
    if (len >= max_len)
        len = max_len - 1;
    memmove(dst, src, len);
    dst[len] = '\0';
    return len;
}

/* Calcula el CRC-8 de los bytes usando el valor inicial del TP. */
uint8_t str_crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = STR_CRC8_INIT;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x80)
                crc = (uint8_t)((crc << 1) ^ 0x07);
            else
                crc = (uint8_t)(crc << 1);
        }
    }
    return crc;
}
