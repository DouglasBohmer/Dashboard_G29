"""Envia b pela serial sem resetar o ESP32 e salva apenas o relatorio TFT-B."""
import argparse
from pathlib import Path
import time
import serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--porta", default="COM3")
    parser.add_argument("--tempo", type=float, default=90)
    parser.add_argument("--saida", type=Path, default=Path(".pio/testes/diagnostico_tft_lote.log"))
    args = parser.parse_args()
    if args.tempo <= 0:
        parser.error("--tempo deve ser positivo")
    porta = serial.Serial()
    porta.port, porta.baudrate, porta.timeout = args.porta, 115200, 0.25
    porta.dtr = porta.rts = False
    linhas = []
    iniciou = terminou = False

    def receber():
        nonlocal terminou
        linha = porta.readline().decode("utf-8", errors="replace").strip()
        if "[TFT-B]" in linha:
            linha = linha[linha.index("[TFT-B]"):]
            linhas.append(linha)
            print(linha, flush=True)
            terminou |= linha.startswith("[TFT-B] FIM")
        elif "Guru Meditation" in linha or "Brownout" in linha or "rst:" in linha:
            linhas.append(linha)
            print(linha, flush=True)

    try:
        porta.open()
        porta.reset_input_buffer()
        porta.write(b"b\n")
        porta.flush()
        iniciou = True
        limite = time.monotonic() + args.tempo
        while time.monotonic() < limite and not terminou:
            receber()
    finally:
        if porta.is_open:
            if iniciou and not terminou:
                porta.write(b"x\n")
                limite = time.monotonic() + 6
                while time.monotonic() < limite and not terminou:
                    receber()
            porta.close()
        args.saida.parent.mkdir(parents=True, exist_ok=True)
        args.saida.write_text("\n".join(linhas) + "\n", encoding="utf-8")
        print(f"Relatorio: {args.saida.resolve()}", flush=True)
    if not terminou:
        raise SystemExit("A bateria nao concluiu dentro do limite; cancelamento enviado.")
    if linhas[-1].startswith("[TFT-B] FIM CANCELADO"):
        raise SystemExit("Bateria cancelada; o relatorio e parcial.")


if __name__ == "__main__":
    main()
