"""Compara o ID do LCD antes/depois de resets EN do ESP32, sem cortar o USB."""
import argparse
from pathlib import Path
import time
import serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--porta", default="COM3")
    parser.add_argument("--ciclos", type=int, default=5)
    parser.add_argument("--saida", type=Path, default=Path(".pio/testes/comparacao_reset_esp32.log"))
    args = parser.parse_args()
    if not 1 <= args.ciclos <= 10:
        parser.error("--ciclos deve estar entre 1 e 10")
    porta = serial.Serial()
    porta.port, porta.baudrate, porta.timeout = args.porta, 115200, 0.2
    porta.dtr = porta.rts = False
    linhas = []

    def registrar(linha):
        linhas.append(linha)
        print(linha, flush=True)

    def receber(segundos):
        resposta = ""
        limite = time.monotonic() + segundos
        while time.monotonic() < limite:
            linha = porta.readline().decode("utf-8", errors="replace").strip()
            if "[TFT]" in linha or "rst:" in linha or "Brownout" in linha or "Guru Meditation" in linha:
                registrar(linha)
                if "CONSULTA | ID=" in linha:
                    resposta = linha
        return resposta

    def consultar():
        porta.write(b"d\n")
        porta.flush()
        resposta = receber(2)
        if not resposta:
            return None
        campos = resposta.split("ID=", 1)[1].split("|", 1)[0].split()
        dados = [int(campo, 16) for campo in campos]
        return any(dados[i:i + 2] == [0x93, 0x41] for i in range(len(dados) - 1))

    try:
        porta.open()
        registrar("[RESET] BASE: consulta sem reinicializar.")
        receber(1)
        base = consultar()
        if base is not True:
            registrar("[RESET] Base sem ID valido. Nenhum reset foi executado.")
            return
        aprovados = 0
        for indice in range(1, args.ciclos + 1):
            registrar(f"[RESET] {indice}/{args.ciclos}: pulso EN pelo RTS; USB permanece alimentado.")
            # Mesma sequencia HardReset do esptool para USB-UART: EN baixo por 100 ms.
            porta.dtr = False
            porta.rts = True
            time.sleep(0.1)
            porta.rts = False
            receber(3.5)
            resultado = consultar()
            if resultado is not True:
                registrar("[RESET] ID deixou de responder como 9341. Sequencia interrompida; sem novos resets.")
                break
            aprovados += 1
            registrar(f"[RESET] {indice}/{args.ciclos}: ID compativel apos o reset; imagem precisa ser observada.")
        registrar(f"[RESET] RESUMO: {aprovados}/{args.ciclos} reinicios planejados com ID compativel.")
        registrar("[RESET] EN reinicia o ESP32; o teste nao mede a tensao nem aplica reset fisico no LCD.")
    finally:
        if porta.is_open:
            try:
                porta.rts = False
            finally:
                porta.close()
        args.saida.parent.mkdir(parents=True, exist_ok=True)
        args.saida.write_text("\n".join(linhas) + "\n", encoding="utf-8")
        print(f"Relatorio: {args.saida.resolve()}", flush=True)


if __name__ == "__main__":
    main()
