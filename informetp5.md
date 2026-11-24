# informe tp5

# hito 1

Se divide de esta forma pora poder organizar mejor todo , acelerar el proceso de compilacion y reutilizar codigo mas facilmente

# hito 2

tuve que leer el dasheet

El datasheet indica que los 8 bits menos significativos del registro de 32 bits son los válidos para los LED. También investigué el orden de bytes de la plataforma y comprobé que es little-endian.

![](/home/enzo/snap/marktext/9/.config/marktext/images/2026-11-16-17-28-17-image.png)

Para un índice inválido, las funciones no modifican el registro y devuelven `LED_ERR_IDX`. Si la operación se realiza, devuelven `LED_OK`. Elegimos esta política para que el programa pueda detectar si la solicitud fue válida sin depender de una comprobación visual, y para evitar modificar bits fuera del rango de LED.

# 3

## 3a



![](/home/enzo/snap/marktext/9/.config/marktext/images/2026-11-16-18-07-12-image.png)