#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_ILI9341.h>
#include "configuracao_tela.h"
#include "resultado_diagnostico_tft.h"

namespace DiagnosticoTFT {
// Executa uma etapa por passagem no loop. Nunca alterna os pinos fixos CS/RESET.
class Lote {
 public:
  using Acao = void (*)(bool);
  Lote(Adafruit_ILI9341 &display, Acao reduzirCarga, Acao backlight)
      : hw(display), soft(Configuracao::PINO_CS, Configuracao::PINO_DC,
                         Configuracao::PINO_MOSI, Configuracao::PINO_SCK,
                         Configuracao::PINO_RESET, Configuracao::PINO_MISO),
        carga(reduzirCarga), luz(backlight) {}
  bool ativa() const { return emAndamento; }
  void iniciar() {
    if (emAndamento) return;
    resumo = {}; fase = 0; emAndamento = true; proxima = millis();
    Serial.println("\n[TFT-B] INICIO: bateria automatica. Observe cores e letras. x cancela.");
    Serial.println("[TFT-B] Registros nao comprovam imagem; 00/FF nao comprovam defeito.");
  }
  bool atualizar() {
    if (!emAndamento || int32_t(millis() - proxima) < 0) return false;
    uint32_t espera = 250;
    if (fase >= 1 && fase <= 4) {
      constexpr uint32_t frequencias[] = {100000, 500000, 1000000, 4000000};
      const uint32_t freq = frequencias[fase - 1];
      Serial.printf("[TFT-B] %u/17: leituras repetidas a %lu Hz.\n", fase,
                    static_cast<unsigned long>(freq));
      hw.setSPISpeed(freq); coletar(hw, "VARREDURA"); leituraDireta(freq);
    } else switch (fase) {
      case 0: {
        unsigned altos = 0;
        for (unsigned i = 0; i < 64; ++i) { altos += digitalRead(Configuracao::PINO_MISO) == HIGH; delayMicroseconds(10); }
        Serial.printf("[TFT-B] 0/17: MISO GPIO35 em repouso: %u/64 amostras altas. Sem pull-up interno.\n", altos);
        hw.setSPISpeed(500000); coletar(hw, "BASE"); break;
      }
      case 5:
        Serial.println("[TFT-B] 5/17: reinicializacao conservadora a 100 kHz.");
        hw.begin(100000); hw.setRotation(1); coletar(hw, "INIT 100K");
        hw.setSPISpeed(1000000); desenharPadrao(hw, "HW 100K->1M", true); espera = 1200; break;
      case 6:
        Serial.println("[TFT-B] 6/17: comando Sleep Out e espera de 150 ms.");
        hw.sendCommand(ILI9341_SLPOUT); espera = 150; break;
      case 7: {
        Serial.println("[TFT-B] 7/17: modo normal, RGB565 e Display On explicitos.");
        const uint8_t pixel = 0x55;
        hw.sendCommand(ILI9341_NORON); hw.sendCommand(ILI9341_PIXFMT, &pixel, 1);
        hw.sendCommand(ILI9341_DISPON); coletar(hw, "WAKE");
        desenharPadrao(hw, "WAKE 1M", true); espera = 1200; break;
      }
      case 8:
        Serial.println("[TFT-B] 8/17: padrao visual com escrita a 4 MHz.");
        hw.setSPISpeed(4000000); desenharPadrao(hw, "HW 4M", true); espera = 1200; break;
      case 9:
        Serial.println("[TFT-B] 9/17: verificacao de escrita/leitura do registrador MADCTL.");
        hw.setSPISpeed(500000); testarRegistrador(); break;
      case 10:
        Serial.println("[TFT-B] 10/17: gravacao/leitura de tres pixels RGB na RAM.");
        testarRam(); break;
      case 11:
        Serial.println("[TFT-B] 11/17: Wi-Fi e LEDs laterais/RPM desligados temporariamente.");
        carga(true); cargaReduzida = true; espera = 1000; break;
      case 12:
        hw.begin(100000); hw.setRotation(1); coletar(hw, "SEM WIFI/LEDS");
        hw.setSPISpeed(1000000); desenharPadrao(hw, "SEM WIFI/LEDS", true); espera = 1200; break;
      case 13:
        Serial.println("[TFT-B] 13/17: backlight desligado por 700 ms para comparar as leituras.");
        luz(false); luzApagada = true; espera = 700; break;
      case 14:
        hw.setSPISpeed(500000); coletar(hw, "SEM BACKLIGHT");
        luz(true); luzApagada = false; espera = 700; break;
      case 15:
        Serial.println("[TFT-B] 15/17: SPI por software, usando os mesmos fios e pinos.");
        SPI.end(); usandoSoftware = true; soft.begin(); soft.setRotation(1);
        coletar(soft, "SPI SOFTWARE");
        desenharPadrao(soft, "SPI SOFTWARE", false); espera = 1800; break;
      case 16:
        Serial.println("[TFT-B] 16/17: reconstruindo o periferico SPI com MISO=35, MOSI=23, SCK=18.");
        restaurarSpi(); coletar(hw, "SPI RECRIADO");
        desenharPadrao(hw, "SPI RECRIADO", true); espera = 1200; break;
      case 17:
        finalizar(false); return true;
    }
    ++fase; proxima = millis() + espera;
    return false;
  }
  void cancelar() { if (emAndamento) finalizar(true); }

 private:
  Adafruit_ILI9341 &hw;
  Adafruit_ILI9341 soft;
  Acao carga, luz;
  Resumo resumo;
  bool emAndamento = false, cargaReduzida = false, luzApagada = false, usandoSoftware = false;
  uint8_t fase = 0;
  uint32_t proxima = 0;

  static Leitura ler(Adafruit_ILI9341 &tela) {
    Leitura dados{};
    for (uint8_t i = 0; i < 4; ++i) dados[i] = tela.readcommand8(0xD3, i);
    dados[4] = tela.readcommand8(ILI9341_RDMODE);
    dados[5] = tela.readcommand8(ILI9341_RDPIXFMT);
    dados[6] = tela.readcommand8(ILI9341_RDMADCTL);
    dados[7] = tela.readcommand8(ILI9341_RDSELFDIAG);
    return dados;
  }
  void coletar(Adafruit_ILI9341 &tela, const char *nome) {
    Leitura primeira{}; bool estavel = true;
    for (uint8_t i = 0; i < 3; ++i) {
      const auto amostra = ler(tela); resumo.registrar(amostra);
      if (i == 0) primeira = amostra;
      else estavel &= amostra == primeira;
    }
    Serial.printf("[TFT-B] %-17s ID=%02X %02X %02X %02X modo=%02X pixel=%02X mad=%02X self=%02X estavel=%s\n",
                  nome, primeira[0], primeira[1], primeira[2], primeira[3], primeira[4],
                  primeira[5], primeira[6], primeira[7], estavel ? "3/3" : "NAO");
  }
  static void lerDireto(uint32_t freq, uint8_t registro, uint8_t *saida, size_t quantidade) {
    SPI.beginTransaction(SPISettings(freq, MSBFIRST, SPI_MODE0));
    digitalWrite(Configuracao::PINO_DC, LOW); SPI.transfer(registro);
    digitalWrite(Configuracao::PINO_DC, HIGH);
    for (size_t i = 0; i < quantidade; ++i) saida[i] = SPI.transfer(0);
    SPI.endTransaction();
  }
  static void leituraDireta(uint32_t freq) {
    uint8_t id[8]{}, modo[2]{};
    // Inclui o byte dummy. Metodo direto, sem o comando de indice 0xD9 da biblioteca.
    lerDireto(freq, 0xD3, id, sizeof(id)); lerDireto(freq, ILI9341_RDMODE, modo, sizeof(modo));
    Serial.printf("[TFT-B] DIRETO dummy+dados=%02X %02X %02X %02X %02X %02X %02X %02X modo=%02X %02X\n",
                  id[0], id[1], id[2], id[3], id[4], id[5], id[6], id[7], modo[0], modo[1]);
  }
  static void desenharPadrao(Adafruit_ILI9341 &tela, const char *nome, bool limpar) {
    if (limpar) tela.fillScreen(ILI9341_BLACK);
    tela.setFont(nullptr); tela.setTextSize(2); tela.setTextWrap(false);
    tela.setTextColor(ILI9341_WHITE, ILI9341_BLACK); tela.setCursor(8, 8); tela.print(nome);
    tela.fillRect(8, 50, 48, 48, ILI9341_RED);
    tela.fillRect(72, 50, 48, 48, ILI9341_GREEN);
    tela.fillRect(136, 50, 48, 48, ILI9341_BLUE);
    Serial.printf("[TFT-B] VISUAL enviado: %s (letras e tres quadrados RGB).\n", nome);
  }
  bool idConfirmado() { const auto amostra = ler(hw); return contem9341(amostra.data(), 4); }
  void testarRegistrador() {
    if (!idConfirmado()) { Serial.println("[TFT-B] MADCTL: inconclusivo; ID 9341 nao confirmado. Etapa pulada."); return; }
    const uint8_t a = 0x48, b = 0x28;
    hw.sendCommand(ILI9341_MADCTL, &a, 1); const uint8_t ra = hw.readcommand8(ILI9341_RDMADCTL);
    hw.sendCommand(ILI9341_MADCTL, &b, 1); const uint8_t rb = hw.readcommand8(ILI9341_RDMADCTL);
    hw.setRotation(1);
    Serial.printf("[TFT-B] MADCTL escrito=48/28 lido=%02X/%02X coincidiu=%s.\n", ra, rb,
                  ra == a && rb == b ? "SIM" : "NAO/INCONCLUSIVO");
  }
  void testarRam() {
    if (!idConfirmado()) { Serial.println("[TFT-B] RAM: inconclusiva; ID 9341 nao confirmado. Etapa pulada."); return; }
    constexpr uint16_t cores[] = {ILI9341_RED, ILI9341_GREEN, ILI9341_BLUE};
    for (uint16_t cor : cores) {
      hw.drawPixel(2, 2, cor);
      hw.startWrite(); hw.setAddrWindow(2, 2, 1, 1); hw.endWrite();
      uint8_t rgb[4]{}; lerDireto(500000, ILI9341_RAMRD, rgb, sizeof(rgb));
      const uint16_t lido = (uint16_t(rgb[1] & 0xF8) << 8) | (uint16_t(rgb[2] & 0xFC) << 3) | (rgb[3] >> 3);
      Serial.printf("[TFT-B] RAM escrito=%04X lido=%04X coincidiu=%s.\n", cor, lido,
                    lido == cor ? "SIM" : "NAO/INCONCLUSIVO");
    }
  }
  void restaurarSpi() {
    SPI.end(); SPI.begin(Configuracao::PINO_SCK, Configuracao::PINO_MISO,
                         Configuracao::PINO_MOSI, Configuracao::PINO_CS);
    hw.begin(1000000); hw.setRotation(1); usandoSoftware = false;
  }
  void finalizar(bool cancelado) {
    if (cancelado) delay(150);  // Respeitar eventual Sleep Out ainda em curso.
    if (usandoSoftware || cancelado) restaurarSpi();
    if (luzApagada) { luz(true); luzApagada = false; }
    if (cargaReduzida) { carga(false); cargaReduzida = false; }
    hw.setSPISpeed(1000000); emAndamento = false;
    Serial.printf("[TFT-B] RESUMO amostras=%u ID9341=%u zeradas=%u FF=%u.\n",
                  resumo.amostras, resumo.compativeis, resumo.zeros, resumo.altos);
    Serial.println("[TFT-B] Leituras nao medem a tensao de 3V3 nem confirmam a imagem no vidro.");
    Serial.println("[TFT-B] CS=GND e RESET=3V3: sem possibilidade de pulso fisico nesses pinos por software.");
    Serial.println(cancelado ? "[TFT-B] FIM CANCELADO. Retomando o painel a 1 MHz."
                             : "[TFT-B] FIM. Retomando o painel a 1 MHz. Informe se viu alguma cor/letra.");
  }
};
}  // namespace DiagnosticoTFT
