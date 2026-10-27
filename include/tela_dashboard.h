#pragma once

#include <Arduino.h>
#include <Adafruit_ILI9341.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include "dashboard_f1.h"

namespace InterfaceDashboard {
constexpr float CAPACIDADE_BATERIA_J = 4000000.0f;
constexpr float LIMITE_ENERGIA_VOLTA_J = 4000000.0f;

inline String formatarTempo(uint32_t tempo) {
  if (!tempo || tempo >= 86400000UL) return "--:--.---";
  char texto[20];
  snprintf(texto, sizeof(texto), "%lu:%02lu.%03lu",
           static_cast<unsigned long>(tempo / 60000),
           static_cast<unsigned long>((tempo / 1000) % 60),
           static_cast<unsigned long>(tempo % 1000));
  return texto;
}

inline String formatarIntervalo(const DashboardF1::Intervalo &intervalo) {
  if (!intervalo.valido) return "--";
  if (intervalo.voltas) return "+" + String(intervalo.voltas) + "V";
  char texto[24];
  snprintf(texto, sizeof(texto), "+%lu.%03lu s",
           static_cast<unsigned long>(intervalo.milissegundos / 1000),
           static_cast<unsigned long>(intervalo.milissegundos % 1000));
  return texto;
}

inline const char *nomeModo(uint8_t modo) {
  switch (modo) {
    case 0: return "NENHUM";
    case 1: return "MEDIO";
    case 2: return "VOLTA RAPIDA";
    case 3: return "ULTRAPASSAGEM";
    default: return "--";
  }
}

class Tela {
 public:
  explicit Tela(Adafruit_ILI9341 &display) : tela(display), imagem(320, 108) {}
  bool memoriaDisponivel() const { return imagem.getBuffer() != nullptr; }

  void iniciar() {
    tela.setRotation(1);
    tela.fillScreen(ILI9341_BLACK);
    tela.setTextWrap(false);
    tela.drawFastHLine(0, 26, 320, ILI9341_DARKGREY);
    tela.drawFastHLine(0, 201, 320, ILI9341_DARKGREY);
    tela.drawFastVLine(107, 203, 29, ILI9341_DARKGREY);
    tela.drawFastVLine(213, 203, 29, ILI9341_DARKGREY);
    tela.setFont(nullptr);
    tela.setTextSize(1);
    tela.setTextColor(ILI9341_WHITE);
    const struct { int16_t x, y; const char *texto; } rotulos[] = {
        {44, 121, "km/h"}, {246, 54, "FRENTE"}, {249, 99, "ATRAS"},
        {39, 205, "VOLTA"}, {125, 205, "MINHA MELHOR"}, {231, 205, "MELHOR PROVA"}};
    for (const auto &rotulo : rotulos) {
      tela.setCursor(rotulo.x, rotulo.y);
      tela.print(rotulo.texto);
    }
    for (uint8_t indice = 0; indice < 15; ++indice) coresRpm[indice] = 0xFFFF;
  }

  void atualizar(const DashboardF1::Telemetria &dados, uint32_t agora,
                 const String &situacao, bool permitirDados) {
    const bool carroValido = permitirDados && dados.carroRecente(agora);
    const bool voltaValida = carroValido && dados.voltaRecente(agora);
    const bool energiaValida = carroValido && dados.energiaRecente(agora);
    const bool sessaoValida = dados.sessaoRecente(agora);
    const auto &carro = dados.carro;

    campo(3, 4, 72, 104, 44, carroValido ? String(carro.velocidadeKmh) : "--",
          ILI9341_WHITE, ILI9341_BLACK, &FreeSansBold24pt7b);
    String marcha = "--";
    if (carroValido) marcha = carro.marcha == -1 ? "R" : carro.marcha == 0 ? "N" : String(carro.marcha);
    campo(4, 112, 45, 96, 106, marcha, ILI9341_WHITE, ILI9341_BLACK,
          &FreeSansBold24pt7b, marcha.length() == 1 ? 2 : 1);
    campo(2, 224, 30, 92, 16, carroValido ? "RPM " + String(carro.rpm) : "RPM --");
    desenharRpm(carroValido ? carro.luzesRpm : -1);
    campo(11, 2, 214, 104, 17,
          voltaValida ? formatarTempo(dados.volta.tempo) : "--:--.---",
          voltaValida && dados.volta.invalida ? ILI9341_RED : ILI9341_WHITE,
          ILI9341_BLACK, nullptr, 2);

    if (!haInformacoes || agora - ultimasInformacoes >= 100) {
      haInformacoes = true;
      ultimasInformacoes = agora;
      char posicao[8];
      snprintf(posicao, sizeof(posicao), "P%02u", voltaValida ? dados.volta.posicao : 0);
      campo(0, 4, 3, 66, 22, voltaValida && dados.volta.posicao ? posicao : "P--",
            ILI9341_RED, ILI9341_BLACK, nullptr, 2);
      campo(1, 113, 3, 203, 22,
            "VOLTA " + (voltaValida ? String(dados.volta.numero) : "--") + "/" +
                (sessaoValida && dados.totalVoltas ? String(dados.totalVoltas) : "--"),
            ILI9341_RED, ILI9341_BLACK, nullptr, 2);
      campo(5, 216, 66, 100, 26,
            voltaValida ? formatarIntervalo(dados.volta.frente) : "--",
            ILI9341_WHITE, ILI9341_BLACK, &FreeSansBold9pt7b);
      campo(6, 216, 111, 100, 26,
            voltaValida ? formatarIntervalo(dados.volta.atras) : "--",
            ILI9341_WHITE, ILI9341_BLACK, &FreeSansBold9pt7b);

      const bool drsAtivo = carroValido && carro.drsAtivo;
      const bool drsPronto = energiaValida && dados.energia.drsPermitido;
      campo(7, 6, 156, 96, 38,
            !carroValido ? "DRS --" : drsAtivo ? "DRS ATIVO" : drsPronto ? "DRS PRONTO" : "DRS OFF",
            drsAtivo ? ILI9341_BLACK : drsPronto ? ILI9341_GREEN : ILI9341_LIGHTGREY,
            drsAtivo ? ILI9341_GREEN : drsPronto ? 0x0220 : ILI9341_DARKGREY,
            &FreeSansBold9pt7b);

      // Perfil F1 2025: duas escalas de 4 MJ; VOLTA e uma estimativa do saldo.
      const bool perfilF1 = energiaValida && sessaoValida && dados.formula == 0;
      const int bateria = perfilF1 ? DashboardF1::percentual(dados.energia.armazenada, CAPACIDADE_BATERIA_J) : -1;
      const int gasto = perfilF1 ? DashboardF1::percentual(dados.energia.aplicadaNaVolta, LIMITE_ENERGIA_VOLTA_J) : -1;
      const int saldo = gasto < 0 ? -1 : 100 - gasto;
      campo(8, 108, 153, 80, 14, "VOLTA " + (saldo >= 0 ? String(saldo) + "%" : "--"));
      campo(9, 108, 169, 80, 14, "BATERIA " + (bateria >= 0 ? String(bateria) + "%" : "--"));
      desenharBarra(0, 194, 156, 120, 9, saldo, ILI9341_CYAN);
      desenharBarra(1, 194, 172, 120, 9, bateria, ILI9341_YELLOW);
      campo(10, 108, 185, 208, 13,
            String("MODO: ") + (energiaValida ? nomeModo(dados.energia.modo) : "--"));
      campo(12, 109, 214, 102, 17,
            carroValido ? formatarTempo(dados.minhaMelhor(agora)) : "--:--.---",
            ILI9341_WHITE, ILI9341_BLACK, nullptr, 2);
      campo(13, 215, 214, 103, 17,
            carroValido ? formatarTempo(dados.melhorProva(agora)) : "--:--.---",
            ILI9341_WHITE, ILI9341_BLACK, nullptr, 2);
    }
    campo(14, 0, 232, 320, 8, situacao, carroValido ? ILI9341_LIGHTGREY : ILI9341_YELLOW);
  }

 private:
  struct CampoAnterior {
    String texto;
    uint16_t cor = 0, fundo = 0;
    int16_t x = 0, y = 0, largura = 0, altura = 0;
    bool valido = false;
  } campos[15];
  Adafruit_ILI9341 &tela;
  GFXcanvas1 imagem;
  uint16_t linha[320]{};
  uint16_t coresRpm[15]{};
  int barras[2] = {-2, -2};
  uint32_t ultimasInformacoes = 0;
  bool haInformacoes = false;

  void campo(uint8_t indice, int16_t x, int16_t y, int16_t largura,
             int16_t altura, const String &texto, uint16_t cor = ILI9341_WHITE,
             uint16_t fundo = ILI9341_BLACK, const GFXfont *fonte = nullptr,
             uint8_t tamanho = 1) {
    auto &anterior = campos[indice];
    if (!imagem.getBuffer() || (anterior.valido && anterior.texto == texto &&
        anterior.cor == cor && anterior.fundo == fundo)) return;
    imagem.fillScreen(0);
    imagem.setTextWrap(false);
    imagem.setFont(fonte);
    imagem.setTextSize(tamanho);
    imagem.setTextColor(1);
    int16_t origemX, origemY;
    uint16_t textoLargura, textoAltura;
    imagem.getTextBounds(texto, 0, 0, &origemX, &origemY, &textoLargura, &textoAltura);
    if (textoLargura > largura || textoAltura > altura) {
      imagem.setFont(nullptr);
      imagem.setTextSize(1);
      imagem.getTextBounds(texto, 0, 0, &origemX, &origemY, &textoLargura, &textoAltura);
    }
    const int16_t novoX = (largura - textoLargura) / 2;
    const int16_t novoY = (altura - textoAltura) / 2;
    imagem.setCursor(novoX - origemX, novoY - origemY);
    imagem.print(texto);
    int16_t esquerda = novoX, topo = novoY;
    int16_t direita = novoX + textoLargura, base = novoY + textoAltura;
    if (anterior.valido) {
      esquerda = min(esquerda, anterior.x);
      topo = min(topo, anterior.y);
      direita = max(direita, static_cast<int16_t>(anterior.x + anterior.largura));
      base = max(base, static_cast<int16_t>(anterior.y + anterior.altura));
    }
    if ((!anterior.valido && fundo != ILI9341_BLACK) ||
        (anterior.valido && anterior.fundo != fundo)) {
      esquerda = 0; topo = 0; direita = largura; base = altura;
    }
    esquerda = max<int16_t>(0, esquerda);
    topo = max<int16_t>(0, topo);
    direita = min(direita, largura);
    base = min(base, altura);
    const uint8_t *pixels = imagem.getBuffer();
    tela.startWrite();
    tela.setAddrWindow(x + esquerda, y + topo, direita - esquerda, base - topo);
    for (int16_t linhaY = topo; linhaY < base; ++linhaY) {
      for (int16_t coluna = esquerda; coluna < direita; ++coluna) {
        const bool aceso = pixels[linhaY * 40 + coluna / 8] & (0x80 >> (coluna & 7));
        linha[coluna - esquerda] = aceso ? cor : fundo;
      }
      tela.writePixels(linha, direita - esquerda);
    }
    tela.endWrite();
    anterior = {texto, cor, fundo, novoX, novoY,
                static_cast<int16_t>(textoLargura), static_cast<int16_t>(textoAltura), true};
  }

  void desenharRpm(int percentualRpm) {
    const int acesos = percentualRpm <= 0 ? 0 : (percentualRpm * 15 + 99) / 100;
    for (uint8_t indice = 0; indice < 15; ++indice) {
      const uint16_t cor = indice >= acesos ? ILI9341_DARKGREY :
          indice < 6 ? ILI9341_GREEN : indice < 11 ? ILI9341_YELLOW : ILI9341_RED;
      if (coresRpm[indice] != cor) {
        tela.fillRect(6 + indice * 14, 32, 12, 10, cor);
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
