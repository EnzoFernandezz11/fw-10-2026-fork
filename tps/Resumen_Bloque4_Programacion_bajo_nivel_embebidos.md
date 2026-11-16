# Bloque 4: programación de bajo nivel para embebidos

## Idea general

En un sistema embebido, el procesador no empieza directamente en `main()`. Después del reset ejecuta código de arranque que prepara la memoria y el entorno de C. El *linker* decide dónde estará cada parte del programa; el código accede a los periféricos mediante registros y atiende eventos mediante interrupciones. Finalmente, el ABI define cómo se entienden las funciones compiladas por separado.

El recorrido central es:

**reset → arranque específico de la arquitectura → inicialización del runtime → `main()` → interacción con periféricos e interrupciones.**

## 1. Mapa de memoria

El espacio de direcciones es el conjunto de direcciones que puede usar el procesador. Cada zona está asociada a una memoria o a un dispositivo. Las direcciones concretas dependen del microcontrolador y de su configuración: hay que consultar su documentación.

| Zona o sección | Contenido y función |
|---|---|
| Flash / ROM | Memoria no volátil que suele contener código, constantes y valores iniciales de variables. |
| `.text` | Instrucciones del programa; normalmente se ejecutan desde Flash. |
| `.rodata` | Datos de solo lectura, como muchas constantes; normalmente se ubican en Flash. |
| `.data` | Variables globales o `static` modificables con valor inicial distinto de cero. Se usan en RAM y su imagen inicial se guarda en Flash. |
| `.bss` | Variables globales o `static` sin inicializador explícito, o inicializadas en cero. Ocupan RAM y el arranque las pone en cero. No necesitan almacenar cada cero en la imagen. |
| Stack o pila | Memoria para llamadas, variables locales y contexto guardado; normalmente crece hacia direcciones menores. |
| Heap | Memoria para asignación dinámica; si existe, normalmente crece hacia direcciones mayores. |
| Registros de periféricos | Direcciones asociadas a hardware mediante MMIO; leer o escribir puede controlar o consultar un dispositivo. |

Stack y heap deben tener espacio suficiente y no superponerse. No todas las plataformas usan heap. Las variables locales automáticas suelen estar en el stack o en registros, no en `.bss` o `.data`.

```c
int sin_init;         // normalmente .bss: comienza en 0
int con_init = 42;    // normalmente .data: comienza en 42
const int tabla = 7;  // normalmente .rodata
```

Las secciones exactas dependen del compilador, las opciones y el linker script. `nm archivo.o` permite ver símbolos: típicamente `B` indica `.bss`, `D` datos inicializados, `R` datos de solo lectura y `T` código.

El procesador también tiene registros internos: de propósito general, contador de programa (**PC**), puntero de pila (**SP**) y registros de estado y control. Estos registros del core no son lo mismo que los registros de periféricos mapeados en memoria.

### Alineación y padding

Un dato suele accederse eficientemente cuando su dirección es múltiplo de su tamaño o de la alineación exigida por el ABI. Por ejemplo, un `int` de 4 bytes suele alinearse a 4. Según la arquitectura y la configuración, un acceso desalineado puede ser más lento o provocar una excepción.

El compilador puede insertar bytes de relleno (*padding*) entre campos y al final de una estructura. En un sistema donde `int` ocupa y se alinea a 4 bytes:

```c
struct A { char c; int i; char d; }; // habitualmente 12 bytes
struct B { int i; char c; char d; }; // habitualmente 8 bytes
```

En `A`, `i` suele empezar en el desplazamiento 4 y `d` en el 8. Reordenar campos puede reducir espacio. Un atributo como `packed` elimina relleno según el compilador, pero puede generar accesos desalineados; no conviene usarlo sin considerar el hardware y el formato requerido. `sizeof` y `offsetof` permiten verificar tamaño y desplazamientos reales.

### Endianness

El *endianness* indica el orden de los bytes de un valor multibyte en direcciones crecientes. Para `0x11223344`:

- **Little-endian:** `44 33 22 11`.
- **Big-endian:** `11 22 33 44`.

Esto importa al interpretar buffers, protocolos, archivos y memoria compartida. No cambia el valor lógico del entero, sino su representación en bytes. La arquitectura y la configuración determinan el orden usado.

## 2. Linker script: de las secciones a las direcciones

El compilador genera archivos objeto; el **linker** los combina, resuelve símbolos y asigna direcciones. El *linker script* describe las regiones disponibles y dónde colocar las secciones. Un ejemplo conceptual:

```ld
MEMORY {
  FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 256K
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 64K
}
SECTIONS {
  .text : { *(.text*) *(.rodata*) } > FLASH
  .data : { _sdata = .; *(.data*); _edata = .; } > RAM AT> FLASH
  .bss  : { _sbss = .; *(.bss*); _ebss = .; } > RAM
  _estack = ORIGIN(RAM) + LENGTH(RAM);
}
```

Las direcciones son ilustrativas. Los símbolos delimitan zonas que el código de arranque utiliza: por ejemplo, `_sdata` y `_edata` delimitan `.data` en RAM, y `_sbss` y `_ebss`, `.bss`. Para copiar `.data` también hace falta conocer su dirección de carga en Flash, que puede exponerse con `LOADADDR(.data)`.

- **VMA** (*Virtual Memory Address*): dirección donde se usa la sección durante la ejecución.
- **LMA** (*Load Memory Address*): dirección donde se encuentra su imagen de carga. En `.data`, típicamente la VMA está en RAM y la LMA en Flash; el startup copia entre ambas.
- `NOLOAD` puede reservar una zona sin contenido de carga; `.bss` suele ser de tipo `NOBITS` y se inicializa en RAM durante el arranque.
- `__attribute__((section(".boot")))` puede ubicar un objeto en una sección especial. `KEEP(*(.boot))` evita que el linker descarte esa sección al eliminar secciones no utilizadas.
- `ASSERT(...)` permite hacer fallar el enlace si no se cumple una condición, por ejemplo un límite de tamaño.

Algunos procesadores disponen de memorias locales **ITIM** y **DTIM**, útiles para instrucciones o datos de acceso rápido y predecible. Su disponibilidad y características dependen del dispositivo y pueden describirse como regiones adicionales en el linker script.

Para inspeccionar el resultado: `readelf -S firmware.elf` muestra secciones; `readelf -l firmware.elf`, segmentos y direcciones; `nm firmware.elf`, símbolos; `objdump -h` y `objdump -d`, secciones y desensamblado. Un ELF creado solo para experimentar con direcciones no necesariamente es ejecutable: puede carecer de startup y runtime.

## 3. Del reset a `main()`

El comportamiento exacto depende de la arquitectura, pero el orden conceptual es:

1. **Reset:** el procesador inicia desde el mecanismo de arranque definido por el hardware.
2. **Entrada de arranque:** se identifica el código inicial y, según la arquitectura, se establece la pila y el manejo de excepciones.
3. **Startup** (`start.S`, `crt0` o equivalente): prepara el entorno mínimo de ejecución.
4. **Inicialización del runtime:** copia `.data` desde su imagen en Flash a RAM y llena `.bss` con ceros. También configura lo que haga falta para la plataforma.
5. **Aplicación:** se llama a `main()`; recién entonces el programa C puede asumir que sus variables estáticas están inicializadas.

En **Cortex-M**, en el mapa de arranque visible tras reset, la primera palabra de la tabla de vectores proporciona el SP inicial y la segunda la dirección de `Reset_Handler`. Las siguientes identifican handlers de excepciones e interrupciones. La ubicación efectiva depende del dispositivo y su remapeo.

En **RISC-V**, el core comienza en una dirección de reset definida por la implementación; el registro `mtvec` configura el destino de traps en modo máquina. Su formato y funcionamiento no equivalen a la tabla de reset de Cortex-M.

El startup puede configurar reloj/PLL, FPU, protección de memoria y constructores globales de C++, si corresponde. El orden concreto depende del dispositivo: por ejemplo, una RAM externa debe estar operativa antes de copiar datos hacia ella; los wait-states de Flash deben ser apropiados para la frecuencia antes de aumentarla. No se debe asumir que una secuencia genérica sirve para todos los micros.

## 4. Memoria, periféricos y capas de software

Antes de usar un periférico suele ser necesario:

1. Habilitar su reloj (*clock gating*) y, si corresponde, sacarlo de reset.
2. Configurar el multiplexado de pines y GPIO.
3. Programar sus registros en el orden exigido por el manual.
4. Limpiar estados pendientes y habilitar las interrupciones correspondientes cuando el sistema esté listo.

También puede ser necesario configurar wait-states de Flash, RAM, cachés, MPU/MMU o memorias externas. Cada paso depende del hardware y de la secuencia documentada por el fabricante.

**MMIO** (*memory-mapped I/O*) significa que se controla hardware mediante accesos a direcciones de memoria. Una organización habitual separa:

**aplicación → API del driver o HAL → registros MMIO → periférico físico.**

El driver encapsula detalles del dispositivo y expone operaciones como inicializar, leer o escribir. Los registros se acceden con tipos `volatile` apropiados, para que el compilador no elimine ni reutilice indebidamente accesos que pueden tener efectos externos. `volatile` por sí solo **no** aporta atomicidad, exclusión mutua ni todas las garantías de orden entre CPU y periférico: pueden hacer falta primitivas o barreras específicas.

Una `struct` mapeada a registros debe respetar exactamente offsets, ancho de acceso y campos reservados del manual. También hay que respetar los efectos de lectura/escritura de cada registro: por ejemplo, no siempre es seguro hacer una operación de lectura-modificación-escritura.

## 5. Interrupciones

Una interrupción permite atender un evento sin que el programa principal tenga que consultar continuamente si ocurrió. A grandes rasgos:

**evento → controlador y mecanismo de vectores/traps → ISR (handler) → restauración del contexto → continuación del programa.**

- La **ISR** es la rutina que atiende el evento. El hardware y/o el software guardan y restauran el contexto necesario, según la arquitectura.
- La **latencia** es el tiempo entre el evento y el comienzo de la atención. Importa especialmente cuando hay plazos de tiempo real.
- Las **prioridades** pueden permitir que una interrupción más urgente interrumpa a otra. El comportamiento exacto depende del controlador y de su configuración.
- Conviene que la ISR sea corta: reconocer o limpiar el evento según el dispositivo, guardar los datos imprescindibles y delegar el procesamiento a un bucle principal o tarea mediante una bandera o cola.
- Evitar operaciones bloqueantes o trabajo prolongado dentro de la ISR. El intercambio de datos con el resto del programa necesita sincronización apropiada; `volatile` no reemplaza la atomicidad.

Como analogía, un handler de señal como `SIGINT` interrumpe el flujo normal de un programa en un sistema operativo y luego vuelve. Sirve para visualizar la idea, aunque las señales del sistema operativo no son idénticas a las IRQ de un microcontrolador.

## 6. ABI: contrato binario

El **ABI** (*Application Binary Interface*) es el conjunto de convenciones que permite interoperar a código compilado por separado para una plataforma compatible. Incluye, entre otras cosas:

- Convención de llamada: dónde van argumentos y resultado, y cómo se usa y alinea el stack.
- Registros que la función llamada debe preservar (*callee-saved*) y los que quien llama debe guardar si los necesita (*caller-saved*).
- Tamaño y alineación de tipos, y disposición de estructuras.
- Convenciones de símbolos y enlazado; cuando corresponde, mecanismos de excepciones y *unwinding*.

Por ejemplo, en el ABI **System V de x86-64**, una función `int sumar(int a, int b)` recibe los dos primeros enteros en `edi` y `esi`, y devuelve el resultado en `eax`. En el ABI RISC-V habitual, los recibe en `a0` y `a1` y devuelve en `a0`. Las instrucciones concretas cambian con la optimización, pero deben respetar el contrato aplicable.

Así, `main.o` puede llamar a una función de `lib.o` aunque se hayan compilado por separado. Para que funcione deben coincidir el ABI, la arquitectura y la declaración de la función con su definición; un linker exitoso no garantiza que firmas incompatibles sean correctas. Compartir un encabezado ayuda a evitar discrepancias.

## 7. TP4: bootloader y firmware en simulación

El objetivo es crear un **bootloader** pequeño que arranque primero y transfiera el control al firmware del práctico anterior:

1. Agregar el bootloader al proyecto, con su entrada de arranque y la inicialización mínima que requiera la plataforma.
2. Definir el mapa de memoria: bootloader en la región de arranque y firmware en la dirección acordada. Configurar sus linker scripts y generar una imagen conjunta o imágenes para regiones separadas.
3. Cargar ambas partes en el simulador. El bootloader debe localizar el punto de entrada correcto y transferirle el control según la arquitectura.
4. Verificar que el firmware ejecuta su propio startup y llega a `main()` con el estado esperado. En Cortex-M, el traspaso normalmente debe considerar el SP inicial, el vector de reset y la ubicación de la tabla de vectores; los detalles concretos dependen del dispositivo.
5. Inspeccionar el ELF/mapa y el HEX: ubicar vectores y `Reset_Handler`, comprobar las direcciones de `.data` y `.bss`, y comparar los bytes cargados con la memoria del simulador.

Herramientas útiles: `readelf -S/-l`, `objdump -h/-d`, `nm`, `size` de la toolchain del target y GDB (por ejemplo, `x/32xb DIRECCION` para examinar bytes). El archivo `.map` ayuda a relacionar símbolos y secciones con el linker script. El HEX describe datos para cargar en direcciones; `.bss` normalmente no contiene una imagen de ceros y debe verificarse en RAM después del startup.

## Para quedarse con lo esencial

El linker establece **dónde** va cada cosa; el startup prepara **cómo** debe quedar la memoria antes de `main()`; los drivers permiten controlar hardware por registros; las interrupciones atienden eventos; y el ABI hace que las piezas de código puedan llamarse correctamente. El bootloader del práctico reúne estas ideas porque debe arrancar en una ubicación conocida y entregar el control a otro programa también ubicado e inicializado correctamente.

Fuente: `Bloque4_Programacion_bajo_nivel_embebidos.pdf`, diapositivas 1–20.
