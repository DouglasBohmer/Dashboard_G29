#pragma once
#include "campos_bufferizados.h"
#include "tela_dashboard.h"
#include "cores_dashboard.h"
#include "rodape_conexao.h"
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>

namespace InterfaceDashboard {
class TemperaturasF12025 {
 public:
  TemperaturasF12025(Adafruit_ILI9341 &display, CamposBufferizados &buffer) : tela(display), campos(buffer) {}

  void iniciar() {
    tela.setRotation(1);
    tela.fillScreen(ILI9341_BLACK);
    tela.setTextWrap(false);
    tela.setFont(nullptr);
    tela.setTextSize(1);
    tela.setTextColor(ILI9341_LIGHTGREY);
    const struct { int16_t x, y; const char *texto; } rotulos[] = {
        {88, 5, "TEMPERATURAS - C"}, {4, 24, "DE"}, {304, 24, "DD"},
        {4, 123, "TE"}, {304, 123, "TD"}, {87, 24, "FREIO"}, {204, 24, "FREIO"},
        {87, 123, "FREIO"}, {204, 123, "FREIO"}, {151, 63, "KPH"}, {142, 174, "MARCHA"}};
    for (const auto &rotulo : rotulos) {
      tela.setCursor(rotulo.x, rotulo.y); tela.print(rotulo.texto);
    }
    tela.drawFastHLine(0, 18, 320, ILI9341_DARKGREY);
    tela.drawFastHLine(0, 214, 320, ILI9341_DARKGREY);
    for (uint8_t indice = 0; indice < 4; ++indice) {
      const auto &roda = RODAS[indice];
      tela.drawRect(roda.xPneu - 1, roda.y - 1, 78, 72, ILI9341_DARKGREY);
      tela.drawFastHLine(roda.xPneu, roda.y + 34, 76, ILI9341_DARKGREY);
    }
    campos.reiniciar();
    haInformacoes = false;
  }

  void atualizar(const DashboardF1::Telemetria &dados, uint32_t agora, bool permitirDados,
                 const InformacoesConexao *conexao = nullptr) {
    const bool valido = permitirDados && dados.carroRecente(agora);
    const auto &carro = dados.carro;
    campos.campo(0, 123, 29, 74, 30, valido ? String(carro.velocidadeKmh) : "--",
                 ILI9341_YELLOW, ILI9341_BLACK, &FreeSansBold9pt7b);
    String marcha = "--";
    if (valido) marcha = carro.marcha == -1 ? "R" : carro.marcha == 0 ? "N" : String(carro.marcha);
    campos.campo(1, 123, 80, 74, 88, marcha, ILI9341_WHITE, ILI9341_BLACK,
                 &FreeSansBold24pt7b, marcha.length() == 1 ? 2 : 1);
    if (haInformacoes && agora - ultimasInformacoes < 100) return;
    haInformacoes = true; ultimasInformacoes = agora;
    for (uint8_t indice = 0; indice < 4; ++indice) {
      const auto &roda = RODAS[indice];
      const uint8_t interna = carro.temperaturaPneusInterna[roda.indice];
      const uint8_t externa = carro.temperaturaPneusExterna[roda.indice];
      const uint16_t corInterna = valido ? corTemperaturaPneu(interna) : ILI9341_DARKGREY;
      const uint16_t corExterna = valido ? corTemperaturaPneu(externa) : ILI9341_LIGHTGREY;
      const uint16_t corFreio = valido ? corTemperaturaFreio(carro.temperaturaFreios[roda.indice])
                                      : ILI9341_DARKGREY;
      const uint8_t campo = 2 + indice * 3;
      campos.campo(campo, roda.xPneu, roda.y, 76, 34,
                   String("I ") + (valido ? String(interna) + "C" : "--"),
                   valido ? contrasteTemperatura(corInterna) : ILI9341_LIGHTGREY, corInterna, nullptr, 2);
      campos.campo(campo + 1, roda.xPneu, roda.y + 35, 76, 35,
                   String("E ") + (valido ? String(externa) + "C" : "--"),
                   corExterna, ILI9341_BLACK, nullptr, 2);
      campos.campo(campo + 2, roda.xFreio, roda.y, 38, 70,
                   valido ? String(carro.temperaturaFreios[roda.indice]) : "--",
                   valido ? contrasteTemperatura(corFreio) : ILI9341_LIGHTGREY,
                   corFreio, &FreeSansBold9pt7b);
    }
    const bool energiaValida = valido && dados.energiaRecente(agora);
    const auto estilo = estiloErs(energiaValida, dados.energia.modo);
    if (conexao) desenharRodapeConexao(campos, energiaValida, dados.energia.modo, agora, *conexao);
    else campos.campo(14, 0, 216, 320, 24,
                      String("ERS: ") + (energiaValida ? nomeModo(dados.energia.modo) : "--"),
                      estilo.texto, estilo.fundo, nullptr, 2);
  }

 private:
  struct RodaNaTela { F1::Roda indice; int16_t xPneu, xFreio, y; };
  inline static constexpr RodaNaTela RODAS[] = {
      {F1::DianteiraEsquerda, 4, 83, 36}, {F1::DianteiraDireita, 240, 200, 36},
      {F1::TraseiraEsquerda, 4, 83, 135}, {F1::TraseiraDireita, 240, 200, 135}};
  Adafruit_ILI9341 &tela;
  CamposBufferizados &campos;
  bool haInformacoes = false;
  uint32_t ultimasInformacoes = 0;
};
}  // namespace InterfaceDashboard
