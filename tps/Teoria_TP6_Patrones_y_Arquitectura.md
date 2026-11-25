# Teoría para el TP6 — Patrones de Diseño en Embebidos, Control de Flujo y Procesador de Comandos

> Guía teórica y práctica pensada como soporte de [tp6.md](tp6.md). Cubre los fundamentos de comunicaciones/protocolos del TP y los **patrones de diseño en C para sistemas embebidos** que se aplican (y se evalúan) en el diseño del procesador de comandos.

---

## Idea general

En los prácticos anteriores el foco estuvo en hablar con el hardware desde abajo hacia arriba: arranque, linker scripts y drivers aislados (TP4 y TP5). En el **TP6** el foco cambia a la **arquitectura de firmware**: cómo combinar varios drivers detrás de un sistema robusto que recibe un flujo continuo de bytes por una línea serie real (`UART1`), no pierde datos aunque procese más lento que la línea, detecta y valida tramas, se recupera de errores y ejecuta comandos de forma limpia y extensible.

El recorrido conceptual del TP6 tiene tres capas:

```
Línea serie (UART1) ──► Control de flujo (RTS) ──► Sincronización y validación ──► Despacho y ejecución
   (Físico / FIFO)         (Backpressure)              (Parser / FSM)              (Comandos + Drivers)
```

---

## Parte 1 — Fundamentos técnicos del TP6

### 1.1 UART serie real y el problema del Overrun (Hito 1)

A diferencia de `UART0` (un buffer de consola para debug en simulación), `UART1` modela una **UART serie real**:

- Formato **8N1**: 1 bit de start (`0`), 8 bits de datos (LSB primero), sin paridad, 1 bit de stop (`1`). Total: **10 tiempos de bit por byte**.
- Con `BAUD_DIV = 32`, cada byte tarda `10 × 32 = 320` clocks en transmitirse por la línea.
- Tiene **FIFOs de hardware de 16 bytes** en RX y en TX (`UART1_FIFO_DEPTH = 16`).
- Si llega un byte cuando el FIFO RX ya tiene 16 bytes, el byte nuevo **se descarta** y se enciende el flag *sticky* `OVERRUN`.
- Si el bit de stop llega en `0`, el byte se descarta y se enciende `FRAME_ERR`.

**¿Por qué aparece el problema?**
En un sistema embebido sin DMA en la UART, la CPU no puede estar leyendo el FIFO RX todo el tiempo: mientras calcula un CRC, ejecuta un comando (por ejemplo una adquisición de ADC o una copia DMA) o envía una respuesta, el host sigue mandando bytes cada 320 clocks. En apenas `16 × 320 = 5120` clocks el FIFO se llena y empezás a perder bytes.

---

### 1.2 Control de flujo por hardware (RTS/CTS), presupuesto e histéresis

La solución estándar en UART es el **control de flujo RTS/CTS** (*Request To Send / Clear To Send*):

- El firmware controla `REG_UART1_CTRL.RTS` (`1` = *"estoy listo para recibir"*, `0` = *"pará de mandar"*). En el cable sale negado (`rts_n` activo en bajo).
- El host mira `rts_n` **antes de empezar a transmitir cada byte**. Si ya empezó a mandar un byte, lo termina; y además un emisor real puede tener latencia interna y mandar algunos bytes extra en vuelo (**`skid`**, en el testbench `skid = 2`).

#### Presupuesto del FIFO (*budget*)

No alcanza con bajar RTS cuando el FIFO llega a 16 bytes, porque ya es tarde. Hay que bajarlo en un nivel `L_alto` tal que quepan los bytes que todavía pueden entrar después de tomar la decisión:

$$L_{\text{alto}} + B_{\text{proceso}} + K_{\text{vuelo}} \le \text{UART1\_FIFO\_DEPTH} = 16$$

Donde en el Hito 1 (`test_flow_control.c`):
- $K_{\text{vuelo}} = 3$ (1 byte que el host ya tenía en curso + `skid = 2`).
- $B_{\text{proceso}} \approx 3$ (bytes que pueden llegar mientras corre `procesar_lento()` entre dos consultas al nivel del FIFO).
- Entonces $L_{\text{alto}} \le 16 - 3 - 3 = 10$ (por ejemplo bajar RTS cuando `level >= 8` deja margen cómodo).

#### Histéresis (dos umbrales: alto y bajo)

Para volver a habilitar al host (`uart1_set_rts(1)`), se usa un **umbral inferior** $L_{\text{bajo}} < L_{\text{alto}}$ (por ejemplo subir RTS cuando `level <= 4`, o incluso `<= 2`).

```
Nivel del FIFO RX (0..16)
16 ├───────────────────────── Overrun (pérdida de bytes)
   │   margen para skid + tiempo de proceso
 8 ├═════════ L_alto ════════► Si RTS=1 y level >= 8  ──► bajar RTS (0)
   │   zona de histéresis (mantiene el estado actual)
 4 ├───────── L_bajo ────────► Si RTS=0 y level <= 4  ──► subir RTS (1)
 0 └───────────────────────── Vacío
```

**¿Por qué hace falta histéresis?**
Si usaras el mismo umbral para bajar y subir (ej. `>= 8` baja, `< 8` sube), cada vez que leés 1 byte pasaría de 8 a 7, subiría RTS, llegaría 1 byte (o 3 por *skid*), volvería a bajar RTS, etc. Eso genera oscilación continua (*chattering*) y te mantiene peligrosamente pegado al techo del FIFO, donde cualquier ráfaga de *skid* puede provocar overrun.

#### Detalle crítico del hardware de `UART1` en esta plataforma

Según `datasheets/UART1_DATASHEET.md`:
1. Escribir `REG_UART1_CTRL` **sobrescribe ambos bits (`RTS` y `CLR_ERR`) a la vez**. Por eso:
   - El driver debe recordar el estado actual de `RTS` en una variable `static` interna.
   - `uart1_clear_errors()` debe escribir `(s_rts ? UART1_CTRL_RTS : 0) | UART1_CTRL_CLR_ERR` para **no bajar RTS accidentalmente** al limpiar errores.
2. Leer `REG_UART1_RX_DATA` tiene **efecto secundario destructivo**: si `VALID = 1`, saca un byte del FIFO. Siempre se lee en una variable temporal una sola vez.
3. En el **Hito 2**, además de controlar el nivel del FIFO durante la recepción, tenés que **bajar RTS explícitamente antes de entrar en cualquier tramo largo sin leer la UART** (verificar CRC de tramas largas, ejecutar el comando, enviar la respuesta) y volver a subirlo al retomar la lectura.

---

### 1.3 Protocolo orientado a tramas (*Framing*), integridad y recuperación (Hito 2)

Por un cable serie solo viaja un **flujo continuo de bytes sin fronteras** (*byte stream*). Si el receptor arranca a mitad de una transmisión o se corrompe un byte, necesita mecanismos para saber dónde empieza y termina cada mensaje:

```
┌──────────┬──────────┬────────┬─────────────┬───────┬──────────┐
│  HEADER  │  LENGTH  │ OPCODE │   PAYLOAD   │  CRC  │  FOOTER  │
│  A5  5A  │  2 B LE  │  1 B   │   N bytes   │  1 B  │  C3  3C  │
└──────────┴──────────┴────────┴─────────────┴───────┴──────────┘
           ◄──────────────────── LENGTH bytes ──────►
                      ◄── CRC-8 (poly 0x07, init 0xFF) ──►
```

Cada campo cumple un rol teórico preciso:

| Mecanismo | Campo en TP6 | Qué problema resuelve |
|---|---|---|
| **Delimitador de inicio** | `HEADER` (`0xA5 0x5A`) | Permite sincronizarse al *stream* e ignorar ruido previo. |
| **Longitud explícita** | `LENGTH` (`2..1032`, LE) | Permite saber cuántos bytes leer cuando el payload es de tamaño variable. |
| **Detección de errores** | `CRC-8` sobre `OPCODE + PAYLOAD` | Detecta bits corruptos antes de ejecutar nada (*validar antes de actuar*). |
| **Delimitador de fin** | `FOOTER` (`0xC3 0x3C`) | Confirma que `LENGTH` era consistente y que el receptor quedó alineado al final. |
| **Resincronización por nuevo inicio** | `0xA5 0x5A` dentro de una trama incompleta | Evita quedar desfasado si el emisor abortó una trama y mandó otra pegada. |
| **Timeout por silencio** | `clock_cycles()` / `clock_elapsed()` | Evita bloqueo eterno (*deadlock*) si una trama queda truncada y dejan de llegar bytes. |

**¿Por qué `clock_elapsed(t0)` funciona aunque el contador de 32 bits desborde?**
En aritmética entera sin signo (`uint32_t`), la resta `now - t0` se calcula módulo $2^{32}$. Mientras el intervalo transcurrido sea menor a $2^{32}$ clocks, el resultado siempre es exacto incluso si `now` dio la vuelta pasando por `0`.

**Seguridad de memoria en bare-metal sin MMU:**
En esta plataforma no hay MMU ni excepción de *bus fault*: un acceso fuera del mapa (`0x0000_0000`–`0x0005_FFFF`) o que cruza regiones **cuelga la CPU indefinidamente**, y escribir `REG_MCU_RESET` (`0x0004_0014`) mantiene el núcleo en reset sin vuelta atrás. Además, en RV32I un acceso de 32 bits desalineado genera *trap*. Por eso el firmware actúa como **guardia de seguridad**: valida alineación (`addr % 4 == 0`), rango completo (cuidando overflow en `addr + size`), pertenencia a una misma región y exclusión de `REG_MCU_RESET` **antes** de tocar el bus.

---

## Parte 2 — Patrones de diseño en sistemas embebidos (en C)

### ¿Por qué importan en firmware embebido?

Los patrones de diseño no son "recetas académicas de Java/C++": son soluciones probadas a problemas recurrentes de arquitectura (acoplamiento, concurrencia, manejo de estados, acceso a hardware único).

En **C bare-metal** (sin sistema operativo ni heap), los patrones se adaptan a las restricciones del entorno:
- **Sin `malloc`/`free`**: toda la memoria es estática (`static`, buffers de tamaño fijo) o en stack acotado.
- **Sin clases ni herencia**: la encapsulación se logra con visibilidad de archivo (`static` en el `.c`), estructuras (`struct`) y **punteros a función** para polimorfismo e inversión de control.
- **Costo determinista**: cada indirección o capa extra debe justificarse por el problema real que resuelve.

A continuación se desarrollan los **5 patrones que viste mencionados** (`Singleton`, `Máquina de Estados`, `Callbacks`, `Adapter`, `Queuing / Productor-Consumidor`) y luego el **análisis del 6.º patrón** (con **Command / Command Processor** como candidato principal para TP6, más los otros candidatos clásicos de cursos de embebidos).

---

### Patrón 1 — Singleton

#### Qué es y qué problema resuelve
Garantiza que exista **una única instancia** de un recurso o gestor y provee un punto de acceso controlado a él.

En sistemas embebidos aparece todo el tiempo porque **el hardware físico es único**: hay una sola `UART1`, un solo `DMA`, un solo `ADC`, un único puerto de LEDs o un único procesador de comandos activo. Si dos módulos mantuvieran copias separadas del estado interno de ese hardware (por ejemplo, dos variables distintas para recordar si `RTS` está en alto o en bajo), el estado del software se desincronizaría del hardware real.

#### Cómo se implementa en C embebido
En C no hace falta una clase con constructor privado: la unidad de traducción (`.c`) + variables `static` ya implementan un Singleton natural, o bien una función `get_instance()` que devuelve un puntero a una estructura `static`:

```c
/* Opción A: Singleton a nivel de módulo (la más común en drivers C) */
/* uart1.c */
static int s_rts_enabled = 0;   /* estado único y privado, invisible fuera de uart1.c */
static int s_initialized = 0;

void uart1_init(uint32_t baud_div) {
    if (baud_div < 2) return;
    REG_UART1_BAUD_DIV = baud_div;
    s_rts_enabled = 0;
    REG_UART1_CTRL = UART1_CTRL_CLR_ERR;
    s_initialized = 1;
}

void uart1_set_rts(int ready) {
    s_rts_enabled = ready ? 1 : 0;
    REG_UART1_CTRL = s_rts_enabled ? UART1_CTRL_RTS : 0u;
}

void uart1_clear_errors(void) {
    REG_UART1_CTRL = (s_rts_enabled ? UART1_CTRL_RTS : 0u) | UART1_CTRL_CLR_ERR;
}
```

```c
/* Opción B: Singleton explícito con struct (útil si querés pasar el puntero de contexto) */
typedef struct {
    int rts_on;
    uint32_t rx_overruns;
} uart1_ctx_t;

uart1_ctx_t *uart1_get_instance(void) {
    static uart1_ctx_t instance = { .rts_on = 0, .rx_overruns = 0 };
    return &instance;
}
```

#### Ventajas y costos (*trade-offs*)
- **Ventajas:** Cero overhead de memoria dinámica; garantiza consistencia del estado de hardware único; interfaz simple.
- **Costos:** Si mañana el SoC agrega `UART2`, hay que refactorizar para aceptar una instancia/handle (`uart_t *dev`). Además, el estado global oculto puede dificultar tests unitarios aislados si no hay una función de reset.

#### Dónde encaja en el TP6
- En `fw/drivers/src/uart1.c` para proteger la única instancia del registro `REG_UART1_CTRL` y mantener sincronizado `s_rts_enabled`.
- Opcionalmente en el contexto único del procesador de comandos (aunque pasar un `parser_t *ctx` suele ser más limpio y testeable).

---

### Patrón 2 — Máquina de Estados Finitos (FSM — *Finite State Machine*)

#### Qué es y qué problema resuelve
Modela un sistema que en cada instante está en **uno de un conjunto finito de estados** y reacciona a **eventos** (llegada de un byte, vencimiento de un timeout) ejecutando acciones y cambiando de estado según reglas explícitas.

Resuelve el caos de tener decenas de banderas sueltas (`viendo_header`, `tengo_len`, `ya_lei_opcode`, `esperando_footer`, `hubo_a5`...) combinadas en `if/else` anidados imposibles de razonar o depurar.

#### Cómo se implementa en C embebido

Hay dos variantes clásicas en C:

##### Variante A: Basada en `enum` + `switch` (ideal para 5–10 estados)
```c
typedef enum {
    ST_WAIT_HDR0,
    ST_WAIT_HDR1,
    ST_WAIT_LEN_LO,
    ST_WAIT_LEN_HI,
    ST_RECV_BODY,       /* OPCODE + PAYLOAD + CRC */
    ST_WAIT_FTR0,
    ST_WAIT_FTR1
} parser_state_t;

typedef struct {
    parser_state_t state;
    uint16_t       length;      /* valor de LENGTH (2..1032) */
    uint16_t       idx;         /* bytes acumulados en body[] */
    uint8_t        body[1032];  /* OPCODE + PAYLOAD + CRC */
    uint8_t        prev_was_a5; /* para detectar 0xA5 0x5A en medio de la trama */
    uint32_t       last_rx_clk; /* marca de tiempo del último byte */
} frame_parser_t;
```

##### Variante B: Basada en punteros a función o tabla de estados (escalable para sistemas grandes)
Cada estado es una función `typedef void (*state_fn_t)(fsm_t *fsm, uint8_t byte);` y el contexto guarda `state_fn_t current_state;`. Cambiar de estado es simplemente `fsm->current_state = st_wait_hdr1;`. Evita un `switch` gigante cuando hay muchos estados.

#### Aplicación directa al TP6 (⭐ patrón clave para el Hito 2)

El receptor de tramas del TP6 es el ejemplo de manual de una FSM:

```
  (otro byte)                          (byte != 0x5A && != 0xA5)
   ┌─────┐                             ┌────────────────────────┐
   ▼     │                             ▼                        │
┌──────────────┐  0xA5   ┌──────────────┐  0x5A   ┌────────────────┐
│  WAIT_HDR0   │────────►│  WAIT_HDR1   │────────►│  WAIT_LEN_LO   │
└──────────────┘         └──────────────┘         └────────────────┘
   ▲                        │    ▲                        │ (cualquier byte)
   │                        └────┘                        ▼
   │                         0xA5                 ┌────────────────┐
   │                                              │  WAIT_LEN_HI   │
   │  TIMEOUT (> SILENCE_CLOCKS)                  └────────────────┘
   │  desde cualquier estado != WAIT_HDR0           │            │
   │  (descarta SIN responder)            len<2 || len>1032   2<=len<=1032
   │                                        (resp ERROR_LEN,     │
   │                                         opcode=0)           ▼
   │◄───────────────────────────────────────────────┘    ┌──────────────┐
   │                                                     │  RECV_BODY   │◄─┐
   │  Si aparece 0xA5 0x5A en LEN/BODY/FTR:              │ (idx < len)  │──┘
   │  responder ERROR_LENGTH (con opcode o 0)            └──────────────┘
   │  y saltar DIRECTO a WAIT_LEN_LO!                            │ (idx == len)
   │                                                             ▼
   │                                                     ┌──────────────┐
   │◄── validar CRC + FOOTER y despachar ────────────────│ WAIT_FTR0/1  │
                                                         └──────────────┘
```

Fijate cómo la FSM hace evidentes los **casos borde** que con código ad-hoc suelen fallar:
1. **`0xA5 0xA5 0x5A`**: en `WAIT_HDR1`, si llega otro `0xA5`, **no** volvés a `WAIT_HDR0` (si lo hicieras, perderías el `0x5A` siguiente); te quedás en `WAIT_HDR1`.
2. **Nuevo `HEADER` (`0xA5 0x5A`) en medio de una trama incompleta**: se detecta, se emite `ERROR_LENGTH` con el `opcode` de la trama interrumpida (o `0` si aún no se recibió) y se transiciona **directamente a `WAIT_LEN_LO`** para procesar la trama nueva sin perder sus bytes.
3. **Timeout por silencio**: solo aplica cuando `state != ST_WAIT_HDR0`. Al dispararse, se resetea a `ST_WAIT_HDR0` sin responder.
4. **No bloqueante**: la FSM procesa **un byte por invocación** (`parser_feed(fsm, byte)`), permitiendo chequear `flow_control()` y `clock_elapsed()` en cada vuelta del lazo principal.

#### Ventajas y costos (*trade-offs*)
- **Ventajas:** Comportamiento completamente determinista, fácil de auditar caso por caso contra la especificación, no bloquea el lazo principal, recuperación limpia ante cualquier error.
- **Alternativa descartada:** Lectura secuencial bloqueante (`read_header(); read_len(); read_n_bytes();`) — se traba con tramas truncadas y no puede detectar un `0xA5 0x5A` inesperado en el medio.
- **Costos:** Algo más verboso al principio que una lectura lineal, y hay que cuidar que todas las transiciones reinicien las variables de contexto (`idx`, `prev_was_a5`, etc.).

---

### Patrón 3 — Callbacks (Retrollamadas / Inversión de Control)

#### Qué es y qué problema resuelve
Un **Callback** es una función que se pasa como referencia (en C, un **puntero a función**) a otro módulo para que ese módulo la invoque cuando ocurra un evento o cuando necesite ejecutar una operación específica, **sin acoplarse a la implementación concreta**.

Resuelve el acoplamiento rígido: un módulo genérico (por ejemplo el *parser* de tramas o el *despachador* de comandos) no necesita `#include` ni conocer los detalles internos de cada comando (`adc.h`, `dma.h`, `led.h`).

#### Cómo se implementa en C embebido

En embebidos hay dos usos muy frecuentes de callbacks:

##### Uso A: Callback de notificación de evento (Parser → Aplicación)
El parser no sabe qué hacer con una trama válida; simplemente llama al callback registrado cuando completa una trama:

```c
typedef void (*on_frame_cb_t)(uint8_t opcode, const uint8_t *payload, uint16_t payload_len);
typedef void (*on_error_cb_t)(uint8_t opcode, uint8_t status_err);

typedef struct {
    /* ... estado de la FSM ... */
    on_frame_cb_t on_frame;
    on_error_cb_t on_error;
} frame_parser_t;
```

##### Uso B: Tabla de Callbacks (*Dispatch Table*) para comandos
Cada opcode apunta a su función manejadora con una firma uniforme:

```c
typedef uint8_t (*cmd_handler_t)(const uint8_t *req_payload, uint16_t req_len,
                                 uint8_t *resp_data, uint16_t *resp_len);

typedef struct {
    uint8_t       opcode;
    cmd_handler_t handler;
} cmd_entry_t;

/* Tabla estática en .rodata (ROM/Flash, no gasta RAM) */
static const cmd_entry_t cmd_table[] = {
    { OP_ADC,       cmd_handle_adc       },
    { OP_DMA,       cmd_handle_dma       },
    { OP_MEM_READ,  cmd_handle_mem_read  },
    { OP_MEM_WRITE, cmd_handle_mem_write },
    { OP_LED,       cmd_handle_led       },
};
```

#### Ventajas y costos (*trade-offs*)
- **Ventajas:** Cumple el principio *Abierto/Cerrado* — para agregar un comando nuevo (ej. `OP_VERSION = 0x06`) solo escribís `cmd_handle_version` y agregás una línea a `cmd_table`, **sin tocar ni el parser ni el despachador**. Permite desacoplar módulos en archivos `.c` separados.
- **Alternativa descartada:** Un `switch (opcode)` monolítico dentro del bucle del parser, que mezcla lógica de comunicación con lógica de periféricos y crece sin control.
- **Costos:** Una llamada indirecta por puntero a función (en RISC-V es una instrucción `jalr`, overhead despreciable frente a los 320 clocks por byte de UART); hay que verificar que el puntero no sea `NULL` antes de llamarlo; el análisis estático de stack puede ser ligeramente más complejo.

#### Dónde encaja en el TP6
- Exactamente en el criterio de la consigna (§5): *"Se valora que agregar un comando nuevo no obligue a reescribir la lógica de sincronización y validación de tramas."*

---

### Patrón 4 — Adapter (Adaptador / HAL)

#### Qué es y qué problema resuelve
Envuelve un componente existente cuya interfaz no coincide con la que espera el cliente y lo expone con una **interfaz uniforme y desacoplada**.

En embebidos es la base de las capas **HAL (*Hardware Abstraction Layer*)** y de la abstracción de canales de transporte: permite que la lógica de aplicación (el procesador de comandos) no dependa de registros MMIO específicos ni de un periférico concreto.

#### Cómo se implementa en C embebido

En C se implementa típicamente con una estructura de operaciones (*vtable* estática) o un módulo envoltorio (*wrapper*):

```c
/* Interfaz genérica de canal de bytes (independiente de si es UART1, SPI, USB o un buffer de test) */
typedef struct {
    int      (*getc)(uint8_t *b);
    void     (*write)(const uint8_t *buf, uint32_t n);
    void     (*set_rts)(int ready);
    uint32_t (*rx_level)(void);
} byte_channel_t;

/* Adaptador concreto que conecta la interfaz genérica con el driver de UART1 */
static const byte_channel_t uart1_adapter = {
    .getc     = uart1_getc,
    .write    = uart1_write,
    .set_rts  = uart1_set_rts,
    .rx_level = uart1_rx_level,
};
```

También los propios drivers del TP5 (`led.c`, `adc.c`, `dma.c`, `uart1.c`) actúan como **adaptadores de hardware**: traducen operaciones de alto nivel (`adc_read`, `dma_copy`, `led_write`) a lecturas/escrituras de registros MMIO con sus secuencias exactas (flancos de `START`, polling de flags, máscaras de bits).

#### Ventajas y costos (*trade-offs*)
- **Ventajas:** Permite cambiar el transporte (de UART1 a otra interfaz) o correr tests unitarios del procesador de comandos en una PC usando un `mock_channel_adapter` en memoria, sin modificar el parser ni los comandos.
- **Alternativa descartada:** Llamar directamente a `REG_UART1_*` o hardcodear `uart1_*` en todo el procesador de comandos.
- **Costos:** Si en el proyecto siempre va a existir únicamente `UART1` y no se van a hacer tests fuera del simulador, una `vtable` completa de transporte agrega indirección sin beneficio inmediato; en ese caso puede bastar con que el propio header `uart1.h` actúe como interfaz adaptadora del hardware. *(Justificar conscientemente si se usa o si se descarta esta indirección extra demuestra criterio de diseño en la evaluación.)*

---

### Patrón 5 — Queuing (Colas / Buffer Circular — Patrón Productor-Consumidor)

#### Qué es y qué problema resuelve
El patrón **Queuing** desacopla en el tiempo a un **Productor** (que genera datos en ráfagas rápidas o asincrónicas) de un **Consumidor** (que los procesa a un ritmo distinto o más lento), interponiendo una **cola FIFO** (*First-In, First-Out*) de tamaño acotado.

```
┌────────────┐  push (rápido / en ráfagas)   ┌───────────────────────┐  pop (ritmo del FW)   ┌────────────┐
│  PRODUCTOR │ ─────────────────────────────►│  COLA FIFO / RING BUF │ ─────────────────────►│ CONSUMIDOR │
│ (Host/ISR) │ ◄────── backpressure (RTS) ───│  [ d0 | d1 | d2 | .. ]│                       │ (App / FW) │
└────────────┘                               └───────────────────────┘                       └────────────┘
```

#### Conceptos clave del Productor-Consumidor en embebidos

1. **Absorción de ráfagas (*burst absorption*):** Aunque el promedio de velocidad del consumidor sea suficiente, los datos suelen llegar en ráfagas. La cola amortigua la diferencia temporal.
2. **Buffer Circular (*Ring Buffer*):** En C embebido una cola FIFO se implementa sobre un arreglo estático de tamaño fijo con dos índices:
   - `head` (o `wr`): posición donde escribe el **productor**.
   - `tail` (o `rd`): posición desde donde lee el **consumidor**.
   - Al llegar al final del arreglo, ambos índices vuelven a `0` (*wrap-around*).
3. **Política ante cola llena (*Overflow / Backpressure*):**
   - *Descartar nuevos* (drop tail — es lo que hace el FIFO de UART1 con `OVERRUN`).
   - *Sobrescribir viejos* (común en logs o trazas).
   - ***Backpressure* (contrapresión):** avisarle al productor que frene antes de que la cola se llene → **en el TP6 esto es exactamente la línea `RTS`**.
4. **Concurrencia (cuando el productor es una ISR):**
   - Si hay un único productor (ISR) y un único consumidor (`main`), el esquema **SPSC** (*Single-Producer Single-Consumer*) puede implementarse sin *locks* pesados siempre que el productor solo modifique `head`, el consumidor solo modifique `tail`, y los índices compartidos estén marcados como `volatile` (y sean atómicos en el ancho de palabra de la arquitectura, como `uint32_t` en RV32I).

#### Implementación clásica de un Ring Buffer en C (sin heap)

```c
#define RB_SIZE 64u   /* idealmente potencia de 2: permite máscara (RB_SIZE - 1) en vez de '%' */

typedef struct {
    uint8_t           buf[RB_SIZE];
    volatile uint32_t head;   /* solo lo escribe el productor */
    volatile uint32_t tail;   /* solo lo escribe el consumidor */
} ring_buf_t;

static inline void rb_init(ring_buf_t *rb) {
    rb->head = 0;
    rb->tail = 0;
}

static inline uint32_t rb_count(const ring_buf_t *rb) {
    return rb->head - rb->tail;  /* aritmética modular sin signo (válido si RB_SIZE es pot. de 2) */
}

static inline int rb_is_empty(const ring_buf_t *rb) {
    return rb->head == rb->tail;
}

static inline int rb_is_full(const ring_buf_t *rb) {
    return rb_count(rb) >= RB_SIZE;
}

/* Productor */
static inline int rb_push(ring_buf_t *rb, uint8_t b) {
    if (rb_is_full(rb)) return 0;               /* cola llena */
    rb->buf[rb->head & (RB_SIZE - 1u)] = b;
    rb->head++;
    return 1;
}

/* Consumidor */
static inline int rb_pop(ring_buf_t *rb, uint8_t *b) {
    if (rb_is_empty(rb)) return 0;              /* cola vacía */
    *b = rb->buf[rb->tail & (RB_SIZE - 1u)];
    rb->tail++;
    return 1;
}
```

> **Truco elegante de índices crecientes con tamaño potencia de 2:** En lugar de hacer `head = (head + 1) % RB_SIZE` (que desperdicia 1 slot para distinguir lleno de vacío y usa división/módulo), se incrementan `head` y `tail` libremente y se indexa con `& (RB_SIZE - 1)`. Así se aprovechan los `RB_SIZE` slots completos, `head - tail` da la ocupación exacta incluso con desborde de `uint32_t`, y no hay instrucción de división (en RV32I sin extensión `M`, `%` llama a una rutina software lenta).

#### Dónde encaja en el TP6
- **Hito 1 (`test_flow_control.c`):** Es literalmente un escenario **Productor-Consumidor**:
  - Productor = host de UART1 (manda 1 byte cada 320 clocks).
  - Cola = FIFO RX hardware de 16 bytes de `UART1`.
  - Consumidor = `procesar_lento()` (tarda ~3 tiempos de byte por dato).
  - Contrapresión (*backpressure*) = `flow_control()` bajando y subiendo `RTS` con histéresis según `uart1_rx_level()`.
- **Hito 2:** Podés analizar explícitamente si conviene agregar una cola software adicional o no:
  - Como no hay interrupciones de UART1 en esta plataforma (el FW hace *polling*) y además `RTS` frena al host mientras se valida y ejecuta cada trama, **el FIFO hardware + el buffer de trama de la FSM + RTS** ya cumplen el rol de cola sin copiar dos veces cada byte.
  - Si en cambio decidieras desacoplar recepción y ejecución en cola de mensajes (*command queue*) o vaciar el FIFO hardware hacia un ring buffer en RAM antes de procesar, **Queuing** te daría un colchón mucho más grande que 16 bytes (por ejemplo 256 o 1024 bytes), a cambio de más RAM y una copia extra por byte.

---

### Patrón 6 — ¿Cuál fue el 6.º patrón de la clase?

Como no tenemos las diapositivas exactas de tu cursada, conviene ser claros: en cursos de patrones de diseño para embebidos en C que dan exactamente **6 patrones** junto con *Singleton, State Machine, Callbacks, Adapter y Queuing*, el **candidato más fuerte para el 6.º** (sobre todo por el tema exacto del TP6) es el **Patrón Command**, aunque a veces el 6.º en otras cátedras es **Observer**, **Facade/Proxy** o **Opaque Pointer**.

Abajo tenés desarrollado **Command** en profundidad (porque aplica de lleno al TP6) y un resumen claro de los otros 3 por si al leerlos reconocés cuál fue exactamente el de la diapositiva.

---

#### 6.A — Patrón Command (*Comando* / *Command Processor*) ⭐ *(el más alineado con TP6)*

##### Qué es y qué problema resuelve
Encapsula una **solicitud de operación** (qué hacer y con qué parámetros) como una entidad estructurada e independiente de:
- **Quién la recibe / invoca** (*Invoker*: el parser de tramas / despachador), y
- **Quién la ejecuta en el hardware** (*Receiver*: los drivers de ADC, DMA, LED, bus de memoria).

Resuelve el problema central del Hito 2: separar **transporte y protocolo** (framing, LENGTH, CRC, FOOTER) de **semántica de negocio** (decodificar parámetros, validar direcciones, operar periféricos y construir la respuesta).

##### Cómo se implementa en C embebido (estructura de Command Processor)

```c
/* Representación desacoplada de una petición ya validada (sin HEADER, CRC ni FOOTER) */
typedef struct {
    uint8_t        opcode;
    const uint8_t *payload;
    uint16_t       payload_len;
} cmd_request_t;

/* Representación de la respuesta antes de empaquetarla en trama */
typedef struct {
    uint8_t  status;        /* STATUS_OK o STATUS_ERROR_* */
    uint8_t *data;          /* buffer donde el comando escribe los bytes de salida */
    uint16_t data_len;      /* cantidad de bytes escritos en data */
} cmd_response_t;

/* Firma común de todos los comandos */
typedef void (*cmd_exec_fn_t)(const cmd_request_t *req, cmd_response_t *resp);

/* Descriptor declarativo de cada comando */
typedef struct {
    uint8_t       opcode;
    cmd_exec_fn_t execute;
} cmd_descriptor_t;
```

El **Command Processor** despacha la petición buscando el `opcode` en la tabla:

```c
static const cmd_descriptor_t g_commands[] = {
    { OP_ADC,       cmd_adc_exec       },
    { OP_DMA,       cmd_dma_exec       },
    { OP_MEM_READ,  cmd_mem_read_exec  },
    { OP_MEM_WRITE, cmd_mem_write_exec },
    { OP_LED,       cmd_led_exec       },
};

void cmd_process(const cmd_request_t *req, cmd_response_t *resp) {
    resp->data_len = 0;
    for (uint32_t i = 0; i < (sizeof(g_commands) / sizeof(g_commands[0])); i++) {
        if (g_commands[i].opcode == req->opcode) {
            g_commands[i].execute(req, resp);
            return;
        }
    }
    resp->status = STATUS_ERROR_UNKNOWN_OPCODE;
}
```

##### Ventajas y costos (*trade-offs*)
- **Ventajas:** Cada comando queda aislado en su propia función (o archivo) con responsabilidad única; el parser jamás conoce qué comandos existen; agregar o quitar un comando no toca el código de sincronización ni de armado de respuestas; se combina de forma natural con **Callbacks** (la tabla de despacho) y con **Queuing** (si quisieras encolar `cmd_request_t` para ejecutarlos luego).
- **Costos:** Pequeña estructura adicional (`cmd_request_t`/`cmd_response_t`) y búsqueda lineal en la tabla (para 5 comandos son ~5 comparaciones: instantáneo).

---

#### 6.B — Otros candidatos frecuentes para el 6.º patrón de clase

Si el 6.º patrón de la clase no se llamaba *Command*, es muy probable que haya sido alguno de estos:

| Patrón | Qué es en C embebido | Diferencia clave con los otros 5 | Dónde aparece en este curso |
|---|---|---|---|
| **Observer** *(Observador / Publish-Subscribe)* | Un emisor (*Subject*) mantiene una lista fija de suscriptores (*Observers*) y les avisa a todos cuando ocurre un evento. | Un **Callback** suele ser 1-a-1; **Observer** es **1-a-N** (varios módulos reaccionan al mismo evento sin conocerse entre sí). | Por ejemplo, notificar simultáneamente al LED de estado, al logger de UART0 y al contador de errores ante una falla de trama. |
| **Hardware Proxy / Facade** *(Proxy o Fachada)* | **Proxy:** representa un recurso de hardware único protegiendo su acceso (validaciones, orden de registros). **Facade:** ofrece una interfaz simple sobre varios subsistemas complejos. | **Adapter** traduce una interfaz A en una interfaz B esperada; **Facade** simplifica varios módulos detrás de una API única; **Proxy** controla y protege el acceso al hardware. | Los drivers del TP5 son *Hardware Proxies* de los periféricos MMIO; un módulo `mem_access.c` que valida rangos/alineación/`REG_MCU_RESET` antes de leer/escribir el bus es un **Protection Proxy** clarísimo. |
| **Opaque Pointer** *(Puntero Opaco / PIMPL en C)* | El `.h` expone solo `typedef struct parser parser_t;` (tipo incompleto) y funciones que reciben `parser_t *`. La definición real de `struct parser` queda oculta en el `.c`. | Es el mecanismo de **encapsulación estricta** en C: impide que otro archivo modifique campos internos por error. | Ocultar el estado interno de la FSM o de un ring buffer para forzar que solo se manipulen mediante su API pública. *(En embebidos sin `malloc`, se suele combinar con un pool estático de instancias o un Singleton.)* |
| **Polling / Interrupt-Driven** *(Superloop + ISR)* | Patrón de arquitectura/concurrencia: el código corre en un *superloop* (`for(;;)`) consultando periféricos (polling) o delegando la captura rápida a una ISR que alimenta una cola/flag para el lazo principal. | Describe el **flujo de control temporal** del sistema completo, no solo la relación entre módulos. | En esta plataforma `main()` usa un **superloop no bloqueante** (polling de `uart1_getc()`, `flow_control()` y `clock_elapsed()`). |

---

## Parte 3 — Arquitectura recomendada para el TP6 y cómo justificarla

En la sección §5 de [tp6.md](tp6.md) la cátedra aclara que **lo más evaluado no es meter muchos patrones a la fuerza, sino elegir bien y saber justificar**:
1. Qué problema concreto resuelve cada patrón elegido.
2. Qué alternativa descartaste y por qué.
3. Qué costo (*trade-off*) tiene tu elección.

### Arquitectura limpia por capas (combinación recomendada)

Una arquitectura muy sólida, simple de explicar y sin sobreingeniería para el TP6 combina **3 patrones con rol clarísimo**:

```
┌─────────────────────────────────────────────────────────────────────────┐
│                             main() (Superloop)                          │
│   1) flow_control()        ← Queuing / Backpressure sobre FIFO RX HW    │
│   2) uart1_getc(&b)        ← Driver UART1 (Singleton / Hardware Proxy)  │
│   3) parser_feed(&fsm, b)  ← Máquina de Estados Finitos (FSM)           │
│   4) parser_check_timeout()← Recuperación por silencio (clock.h)        │
└────────────────────────────────────┬────────────────────────────────────┘
                                     │ callback on_frame(req) / on_error()
                                     ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                 Command Processor (Command + Callbacks)                 │
│   - Baja RTS antes de ejecutar / responder                              │
│   - Busca opcode en cmd_table[] (tabla de punteros a función)           │
│   - Ejecuta cmd_handle_*(req, resp)                                     │
│   - Empaqueta respuesta (HEADER, LENGTH, OPCODE|0x80, STATUS, data,     │
│     CRC, FOOTER) y la transmite por UART1                               │
│   - Vuelve a subir RTS al finalizar                                     │
└──────────────┬───────────────┬──────────────┬──────────────┬────────────┘
               ▼               ▼              ▼              ▼
           cmd_adc         cmd_dma       cmd_mem_rw       cmd_led
         (adc.h/c)       (dma.h/c)   (mem_validate +    (led.h/c)
                                      bus seguro)
```

### Tabla lista para defender decisiones de diseño

| Parte del TP6 | Patrón aplicado | Problema real que resuelve | Alternativa descartada | Costo / Trade-off |
|---|---|---|---|---|
| **Driver `uart1.c`** | **Singleton** (estado `static` de módulo) | Mantener coherente el bit `RTS` de `REG_UART1_CTRL` al limpiar errores (`uart1_clear_errors`) sin leer registros con estado indefinido. | Pasar el estado de RTS como parámetro en cada llamada o confiar en leer `REG_UART1_CTRL`. | Solo soporta una instancia de UART1 (suficiente para esta plataforma). |
| **Control de flujo (Hito 1 y 2)** | **Queuing (Productor-Consumidor con backpressure)** | Evitar `OVERRUN` en el FIFO RX de 16 bytes cuando el firmware procesa más lento que la línea (320 clocks/byte). | Agrandar indefinidamente un buffer en RAM sin frenar al host (falla ante ráfagas arbitrariamente largas). | Reduce temporalmente el throughput al pausar al host con RTS; exige elegir umbrales con histéresis. |
| **Parser de tramas (`frame_parser.c`)** | **Máquina de Estados Finitos (FSM)** | Sincronizar un stream continuo con ruido, `LENGTH` variable, `0xA5 0x5A` inesperados a mitad de trama y timeouts por silencio sin bloquear el loop. | Lectura secuencial bloqueante o múltiples flags booleanas en `if/else` anidados. | Mayor cantidad de código inicial que un parser lineal; requiere cuidado en las transiciones de reset. |
| **Despacho de comandos (`cmd_proc.c`)** | **Command + Callbacks (Dispatch Table)** | Separar parseo/validación de la lógica de cada periférico; permitir agregar comandos nuevos sin tocar el parser ni el empaquetador de respuestas. | `switch (opcode)` gigante acoplado al parser. | Una indirección por puntero a función y pequeña tabla en `.rodata` (costo mínimo). |
| **Validación de memoria (`mem_guard`)** | **Proxy de protección** *(opcional de mencionar)* | Impedir accesos fuera de `0x0000_0000`–`0x0005_FFFF`, cruces de región, accesos desalineados o escritura a `REG_MCU_RESET` que cuelgan la CPU. | Repetir `if`s de validación en `Memory Read`, `Memory Write` y `DMA`. | Pequeña función centralizada de chequeo antes de cada acceso al bus. |

---

## Parte 4 — Checklist de detalles finos del TP6 (para evitar los bugs más comunes)

Antes de implementar o depurar, revisá estos puntos que suelen hacer fallar `test_flow_control` y `test_main`:

1. **`fw/sources.mk`**: sumar `SRC += drivers/src/uart1.c`, `SRC += lib/src/strutils.c` (para `str_crc8`), y cada módulo nuevo que agregues en `fw/app/` (ej. `SRC += app/frame_parser.c`).
2. **`uart1_clear_errors()` sin pisar RTS**: jamás escribir `REG_UART1_CTRL = UART1_CTRL_CLR_ERR;` a secas si `RTS` estaba en `1`, porque eso baja `RTS` y frena al host.
3. **Bajar RTS antes de tramos largos**: en el Hito 2, cuando llega el último byte de una trama (o incluso cuando el FIFO supera el umbral alto), bajá `RTS` **antes** de calcular `str_crc8()` sobre hasta 1031 bytes, ejecutar el comando y mandar la respuesta. Recién cuando terminás de responder, volvés a evaluar/subir `RTS`.
4. **Secuencia `0xA5 0xA5 0x5A`**: en el estado `WAIT_HDR1`, si llega `0xA5`, mantenete en `WAIT_HDR1`.
5. **Trama interrumpida por `0xA5 0x5A`**:
   - Si todavía no se recibió el `OPCODE` (interrumpida durante `LENGTH`), la respuesta `ERROR_LENGTH` lleva `OPCODE_RESP = 0x00 | 0x80 = 0x80`.
   - Si ya se recibió el `OPCODE`, lleva `OPCODE | 0x80`.
   - Después de responder, **no descartes el nuevo `HEADER`**: la FSM debe quedar en `WAIT_LEN_LO` lista para recibir el `LENGTH` de la trama nueva.
6. **Prioridad de errores de trama**: si fallan **a la vez** el CRC y el FOOTER, la consigna exige responder `ERROR_CRC` (no `ERROR_LENGTH`).
7. **Lectura little-endian segura en RV32I**: no hagas `*(uint32_t *)&payload[i]` porque `payload[i]` puede no estar alineado a 4 bytes y disparar un *trap*. Reconstruí enteros byte a byte:
   ```c
   static uint16_t read_u16_le(const uint8_t *p) {
       return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
   }
   static uint32_t read_u32_le(const uint8_t *p) {
       return  (uint32_t)p[0]
            | ((uint32_t)p[1] << 8)
            | ((uint32_t)p[2] << 16)
            | ((uint32_t)p[3] << 24);
   }
   ```
8. **Validación de rangos sin overflow aritmético**: al validar `[addr, addr + n_words * 4 - 1]`, usá las macros de `config.h` (`ADDR_START_INST`, `ADDR_SIZE_INST`, etc.) y chequeá que `addr >= start` y `size_bytes <= (start + region_size) - addr` (así evitás que una suma `addr + size_bytes` desborde 32 bits y parezca válida).
9. **Silencio solo con trama a medias**: anotá `last_rx = clock_cycles()` cada vez que `uart1_getc(&b)` devuelve `1`, y chequeá `clock_elapsed(last_rx) > SILENCE_CLOCKS` (ej. `40000u`, entre `2000` y `150000`) **solo si la FSM no está en `WAIT_HDR0`**.
