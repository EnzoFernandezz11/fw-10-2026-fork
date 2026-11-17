# informe tp5

# hito 1

Se divide de esta forma pora poder organizar mejor todo , acelerar el proceso de compilacion y reutilizar codigo mas facilmente

# hito 2

tuve que leer el dasheet

los 8 bits mas bajos son los validos de un registro para 32 , tuve que invetigar si era big o llite endian y descrubir qeu el proyecto era little

![](/home/enzo/snap/marktext/9/.config/marktext/images/2026-11-16-17-28-17-image.png)

Para un índice inválido, las funciones no modifican el registro y devuelven `LED_ERR_IDX`. Si la operación se realiza, devuelven `LED_OK`. Elegimos esta política para que el programa pueda detectar si la solicitud fue válida sin depender de una comprobación visual, y para evitar modificar bits fuera del rango de LED.

# 3

## 3a



![](/home/enzo/snap/marktext/9/.config/marktext/images/2026-11-16-18-07-12-image.png)