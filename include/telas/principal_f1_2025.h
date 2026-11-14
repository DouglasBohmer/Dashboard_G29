#pragma once

#include "tela_dashboard.h"
#include "indicadores_painel.h"
#include "perfis_dashboard.h"
#include "cores_dashboard.h"
#include "campos_bufferizados.h"
#include "rodape_conexao.h"
#include <Adafruit_ILI9341.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include "dashboard_f1.h"

namespace InterfaceDashboard {
constexpr float CAPACIDADE_BATERIA_J = 4000000.0f;
constexpr float LIMITE_ENERGIA_VOLTA_J = 4000000.0f;

class PrincipalF12025 {
 public:
  PrincipalF12025(Adafruit_ILI9341 &display, CamposBufferizados &buffer) : tela(display), camposTela(buffer) {}

  void iniciar() {
    tela.setRotation(1);
    tela.fillScreen(ILI9341_BLACK);
    tela.setTextWrap(false);
    tela.drawRect(0, 0, 320, 215, ILI9341_DARKGREY);
    tela.drawFastHLine(0, 44, 320, ILI9341_DARKGREY);
    tela.drawFastHLine(0, 135, 320, ILI9341_DARKGREY);
    tela.drawFastHLine(0, 174, 320, ILI9341_DARKGREY);
    tela.drawFastHLine(0, 192, 320, ILI9341_DARKGREY);
    tela.drawFastVLine(46, 0, 44, ILI9341_DARKGREY);
    tela.drawFastVLine(144, 0, 44, ILI9341_DARKGREY);
    tela.drawFastVLine(242, 0, 44, ILI9341_DARKGREY);
    tela.drawFastVLine(95, 45, 90, ILI9341_DARKGREY);
    tela.drawFastVLine(224, 45, 90, ILI9341_DARKGREY);
    tela.drawFastVLine(106, 136, 38, ILI9341_DARKGREY);
    tela.drawFastVLine(213, 136, 38, ILI9341_DARKGREY);
    tela.drawFastVLine(160, 193, 21, ILI9341_DARKGREY);
    tela.setFont(nullptr);
    tela.setTextSize(1);
    tela.setTextColor(ILI9341_LIGHTGREY);
    const struct { int16_t x, y; const char *texto; } rotulos[] = {
        {15, 32, "KPH"}, {77, 32, "ULTIMA"}, {160, 32, "DELTA ULTIMA"},
        {272, 32, "RPM"}, {26, 125, "POSICAO"}, {141, 125, "MARCHA"},
        {256, 125, "VOLTA"}, {145, 164, "ATUAL"}};
    for (const auto &rotulo : rotulos) {
      tela.setCursor(rotulo.x, rotulo.y);
      tela.print(rotulo.texto);
    }
    camposTela.reiniciar();
    barras[0] = barras[1] = -2;
    haInformacoes = false;
    for (auto &cor : coresRpm) cor = 0xFFFF;
  }

  void atualizar(const DashboardF1::Telemetria &dados, uint32_t agora, bool permitirDados,
                 const InformacoesConexao *conexao = nullptr) {
    const bool carroValido = permitirDados && dados.carroRecente(agora);
    const bool voltaValida = carroValido && dados.voltaRecente(agora);
    const bool energiaValida = carroValido && dados.energiaRecente(agora);
    const bool sessaoValida = permitirDados && dados.sessaoRecente(agora);
    const auto &carro = dados.carro;

    campo(3, 2, 3, 43, 24, carroValido ? String(carro.velocidadeKmh) : "--",
          ILI9341_YELLOW, ILI9341_BLACK, &FreeSansBold9pt7b);
    String marcha = "--";
    if (carroValido) marcha = carro.marcha == -1 ? "R" : carro.marcha == 0 ? "N" : String(carro.marcha);
    campo(4, 97, 47, 125, 78, marcha, ILI9341_WHITE, ILI9341_BLACK,
          &FreeSansBold24pt7b, marcha.length() == 1 ? 2 : 1);
    campo(2, 244, 3, 73, 24, carroValido ? String(carro.rpm) : "--",
          ILI9341_YELLOW, ILI9341_BLACK, &FreeSansBold9pt7b);
    desenharRpm(carroValido ? carro.luzesRpm : -1);
    campo(11, 109, 139, 102, 22,
          voltaValida ? formatarTempo(dados.volta.tempo) : "--:--.---",
          voltaValida && dados.volta.invalida ? ILI9341_RED : ILI9341_WHITE,
          ILI9341_BLACK, &FreeSansBold9pt7b);
    campo(13, 146, 3, 94, 24,
          voltaValida ? formatarDelta(dados.volta.delta) : "--",
          !voltaValida || !dados.volta.delta.valido ? ILI9341_LIGHTGREY :
          dados.volta.delta.milissegundos < 0 ? ILI9341_GREEN :
          dados.volta.delta.milissegundos > 0 ? ILI9341_RED : ILI9341_WHITE,
          ILI9341_BLACK, &FreeSansBold9pt7b);

    if (!haInformacoes || agora - ultimasInformacoes >= 100) {
      haInformacoes = true;
      ultimasInformacoes = agora;
      campo(0, 3, 53, 89, 67,
            voltaValida && dados.volta.posicao ? String(dados.volta.posicao) : "--",
            ILI9341_WHITE, ILI9341_BLACK, &FreeSansBold9pt7b, 2);
      campo(1, 227, 53, 91, 68,
            (voltaValida ? String(dados.volta.numero) : "--") + "/" +
                (sessaoValida && dados.totalVoltas ? String(dados.totalVoltas) : "--"),
            ILI9341_WHITE, ILI9341_BLACK, &FreeSansBold9pt7b, 2);
      campo(5, 3, 139, 100, 22,
            voltaValida ? formatarIntervalo(dados.volta.atras) : "--",
            ILI9341_GREEN, ILI9341_BLACK, &FreeSansBold9pt7b);
      campo(6, 216, 139, 101, 22,
            voltaValida ? formatarIntervalo(dados.volta.frente, '-') : "--",
            ILI9341_RED, ILI9341_BLACK, &FreeSansBold9pt7b);
      campo(16, 3, 164, 100, 8,
            voltaValida ? formatarVizinho(dados, false) : "--", ILI9341_LIGHTGREY);
      campo(17, 216, 164, 101, 8,
            voltaValida ? formatarVizinho(dados, true) : "--", ILI9341_LIGHTGREY);

      // Perfil F1 2025: duas escalas de 4 MJ; VOLTA e uma estimativa do saldo.
      const bool perfilF1 = energiaValida && sessaoValida && dados.formula == 0;
      const int bateria = perfilF1 ? DashboardF1::percentual(dados.energia.armazenada, CAPACIDADE_BATERIA_J) : -1;
      const int gasto = perfilF1 ? DashboardF1::percentual(dados.energia.aplicadaNaVolta, LIMITE_ENERGIA_VOLTA_J) : -1;
      const int saldo = gasto < 0 ? -1 : 100 - gasto;
      campo(8, 3, 193, 154, 12, "VOLTA " + (saldo >= 0 ? String(saldo) + "%" : "--"),
            ILI9341_CYAN);
      campo(9, 163, 193, 154, 12, "BATERIA " + (bateria >= 0 ? String(bateria) + "%" : "--"),
            ILI9341_YELLOW);
      desenharBarra(0, 5, 207, 150, 6, saldo, ILI9341_CYAN);
      desenharBarra(1, 165, 207, 150, 6, bateria, ILI9341_YELLOW);
      const auto estilo = estiloErs(energiaValida, dados.energia.modo);
      if (conexao) desenharRodapeConexao(camposTela, energiaValida, dados.energia.modo, agora, *conexao);
      else campo(10, 0, 216, 320, 24,
                 String("ERS: ") + (energiaValida ? nomeModo(dados.energia.modo) : "--"),
                 estilo.texto, estilo.fundo, nullptr, 2);
      campo(15, 48, 3, 94, 24,
            voltaValida ? formatarTempo(dados.volta.anterior) : "--:--.---",
            ILI9341_WHITE, ILI9341_BLACK, &FreeSansBold9pt7b);
    }
  }

 private:
  Adafruit_ILI9341 &tela;
  CamposBufferizados &camposTela;
  uint16_t coresRpm[Indicadores::QUANTIDADE_RPM]{};
  int barras[2] = {-2, -2};
  uint32_t ultimasInformacoes = 0;
  bool haInformacoes = false;

  void campo(uint8_t indice, int16_t x, int16_t y, int16_t largura,
             int16_t altura, const String &texto, uint16_t cor = ILI9341_WHITE,
             uint16_t fundo = ILI9341_BLACK, const GFXfont *fonte = nullptr,
             uint8_t tamanho = 1) {
    camposTela.campo(indice, x, y, largura, altura, texto, cor, fundo, fonte, tamanho);
  }

  void desenharRpm(int percentualRpm) {
    const size_t acesos = Indicadores::quantidadeAcesa(percentualRpm);
    for (uint8_t indice = 0; indice < Indicadores::QUANTIDADE_RPM; ++indice) {
      const uint16_t cor = indice >= acesos ? ILI9341_DARKGREY : Indicadores::CORES_RPM[indice];
      if (coresRpm[indice] != cor) {
        tela.fillRect(3 + indice * 32, 178, 27, 11, cor);
        coresRpm[indice] = cor;
      }
    }
  }
  void desenharBarra(uint8_t indice, int16_t x, int16_t y, int16_t largura,
                     int16_t altura, int valor, uint16_t cor) {
    if (barras[indice] == valor) return;
    const int anterior = barras[indice] < 0 ? 0 : barras[indice] * largura / 100;
    const int atual = valor < 0 ? 0 : valor * largura / 100;
    if (barras[indice] == -2) tela.fillRect(x, y, largura, altura, ILI9341_DARKGREY);
    if (atual > anterior) tela.fillRect(x + anterior, y, atual - anterior, altura, cor);
    if (atual < anterior) tela.fillRect(x + atual, y, anterior - atual, altura, ILI9341_DARKGREY);
    barras[indice] = valor;
  }
};
}  // namespace InterfaceDashboard
