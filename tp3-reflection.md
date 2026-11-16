Durante el TP trabajamos el proceso completo de construcción de un programa en C: desde compilar archivos fuente hasta enlazarlos, inspeccionar el ejecutable y organizar el código en una biblioteca estática.

Primero vimos que cada archivo .c se compila por separado y genera un archivo objeto .o. Ese objeto contiene código máquina, datos y símbolos, aunque todavía necesita enlazarse para formar el ejecutable. Con nm inspeccionamos esos símbolos: por ejemplo, vimos que main.o usa print_str sin definirla y que su definición está en print.o. El enlazador conecta esas referencias cuando construye el programa.

Después implementamos y usamos distintas partes del proyecto:

- Matemática: trabajamos con funciones como factorial, máximo común divisor, raíz entera y detección de números primos.
- Strings: usamos un buffer de tamaño fijo, sin memoria dinámica, y funciones para agregar texto y números. También implementamos str_trim para quitar espacios de los extremos.
- CRC-8: calculamos un valor de control sobre un conjunto de bytes para poder detectar cambios accidentales en los datos.
- UART simulada: configuramos registros, calculamos BRR para 115200 baudios y enviamos "OK". Vimos que, en esta simulación, los bytes se escriben en un registro en memoria y no llegan a un dispositivo real.
- Buffer circular: guardamos y recuperamos bytes usando índices de escritura y lectura. Al llegar al final del arreglo, los índices vuelven al principio.
- Timer: implementamos un contador que suma un milisegundo por cada llamada a timer_tick(). No mide tiempo real por sí solo.
- Trama: juntamos el buffer y el CRC para enviar y validar una trama con un byte de inicio, cuatro bytes de datos y su CRC.

Luego organizamos las pruebas en demo_math, demo_str y demo_embedded, dejando main más claro. En el Makefile agrupamos los objetos de lib/ y hal/ en libutils.a, una biblioteca estática. Usamos ar t para ver sus miembros y nm para encontrar las funciones definidas en ellos.

También comparamos una compilación normal con una optimizada usando size. En nuestro resultado, el tamaño total informado bajó con -O2, aunque bss aumentó. Eso nos mostró que optimizar no significa que todas las secciones tengan que reducirse. Con size -A vimos las secciones individuales y distinguimos el tamaño del archivo en disco de lo que se carga en memoria: .debug_info, por ejemplo, ocupa espacio en el ejecutable, pero normalmente no se carga al ejecutarlo.

Por último, agregamos --version usando argc y argv, y usamos strings para comprobar que el nombre y la versión estaban presentes en el binario.

Como reflexión, el TP ayuda a entender que un programa no pasa directamente de código fuente a ejecutable: hay etapas intermedias, y cada herramienta permite observar una parte. Saber leer símbolos, objetos, bibliotecas y secciones sirve especialmente cuando aparece un error de enlazado o cuando queremos entender qué contiene y cuánto ocupa el programa.
