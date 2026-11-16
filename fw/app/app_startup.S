/*
 * app_startup.S — TP4 (Bloque 4): arranque del FW cuando corre detrás del BL.
 * Reemplaza a ../startup.S (que no se toca).
 *
 */
#include "fw_image.h"

#define REG_FW_STATUS_ADDR  0x00040000      /* project_config.def: REG_FW_STATUS */
#define FW_STATUS_PASS      0xFF

/* ------------------------------------------------------------------------ */
/*  1) Header + vector table del FW (app.ld lo pone al inicio de APP_ROM)    */
/* ------------------------------------------------------------------------ */
    .section .fw_header, "a"
    .balign 4
    .globl  __fw_header
    .type   __fw_header, @object
__fw_header:
    .word   FW_IMAGE_MAGIC          /* 0x00 magic */
    .word   __fw_start              /* 0x04 load_addr */
    .word   __fw_size               /* 0x08 image_size */
    .word   FW_IMAGE_CRC_UNSET      /* 0x0C CRC calculado por mkimage.py */
    .word   _stack_top              /* 0x10 initial_sp */
    .word   Reset_Handler           /* 0x14 reset_handler */
    .size   __fw_header, . - __fw_header

/* ------------------------------------------------------------------------ */
/*  2) Reset_Handler: "De 0 a main" (diapos 5 y 6)                           */
/* ------------------------------------------------------------------------ */
    .section .text.Reset_Handler, "ax"
    .balign 4                       /* contrato: reset_handler alineado a 4 */
    .globl  Reset_Handler
    .type   Reset_Handler, @function
Reset_Handler:
    la sp, _stack_top /* la -> load addres */

    /* 2  */

    la   t0, _sidata       /* origen: valores iniciales en APP_ROM */
    la   t1, _sdata        /* destino: comienzo de .data en RAM */
    la   t2, _edata        /* límite: final de .data en RAM */

    copiar_data:
        bgeu t1, t2, fin_data  /* si destino >= límite, terminar */
        lw   t3, 0(t0)         /* leer una palabra desde el origen */
        sw   t3, 0(t1)         /* escribirla en el destino */
        addi t0, t0, 4         /* avanzar origen 4 bytes */
        addi t1, t1, 4         /* avanzar destino 4 bytes */
        j    copiar_data       /* repetir */

    fin_data:
        la   t0, _sbss
        la   t1, _ebss

    limpiar_bss:
        bgeu t0, t1, fin_bss
        sw   zero, 0(t0)
        addi t0, t0, 4
        j    limpiar_bss

    fin_bss:
        call main
        li   t0, REG_FW_STATUS_ADDR  /* dirección del registro */
        lw   t1, 0(t0)               /* leer su valor */
        li   t2, FW_STATUS_PASS      /* valor esperado: 0xFF */
        beq  t1, t2, fin_ok          /* si es 0xFF, ir al bucle */
        ebreak                       /* si no, provocar trap */

    fin_ok:
        j    fin_ok                  /* esperar si la simulación aún no terminó */
    .size   Reset_Handler, . - Reset_Handler
