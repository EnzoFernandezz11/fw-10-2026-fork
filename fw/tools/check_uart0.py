"""Verifica los bytes de test_uart0: python3 fw/tools/check_uart0.py salida.log."""

import re
import sys
from pathlib import Path


def main():
    if len(sys.argv) != 2:
        raise SystemExit("Uso: python3 fw/tools/check_uart0.py salida.log")

    log = Path(sys.argv[1]).read_bytes()
    mensajes = re.findall(rb"\x1b\[36mUART: (.*?)\x1b\[0m\r?\n", log, re.DOTALL)
    # El sniffer muestra el byte NUL como un mensaje vacio; el test envia un byte por START.
    salida = b"".join(mensaje if mensaje else b"\x00" for mensaje in mensajes)
    inicio = b"[UART0 TEST BEGIN]\n"
    if salida.count(inicio) != 1:
        raise SystemExit("FAIL: falta el inicio del test o esta duplicado")

    esperado = inicio + b"Debug UART0\n" + bytes([0x00, 0x7F, 0x80, 0xFF])
    for n in (1, 255, 256, 257, 300):
        esperado += bytes(ord("a") + j % 26 for j in range(n))
    for i in range(16):
        esperado += bytes([ord("A") + i]) + b"xy"
    esperado += b"init\n[UART0 TEST PASS]\n"

    obtenido = salida[salida.index(inicio):]
    if obtenido != esperado:
        diferencia = next(
            (i for i, (a, b) in enumerate(zip(obtenido, esperado)) if a != b),
            min(len(obtenido), len(esperado)),
        )
        raise SystemExit(
            f"FAIL: salida distinta desde byte {diferencia}; "
            f"recibidos {len(obtenido)}, esperados {len(esperado)}"
        )
    if b"RESULT: PASS" not in log:
        raise SystemExit("FAIL: la simulacion no termino con RESULT: PASS")
    print(f"PASS UART0: {len(esperado)} bytes correctos, sin perdidas ni duplicados")


if __name__ == "__main__":
    main()
