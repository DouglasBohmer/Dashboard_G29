#pragma once
#include <Adafruit_ILI9341.h>

namespace InterfaceDashboard {
// Um unico canvas e cache compartilhado por todas as paginas.
class CamposBufferizados {
 public:
  explicit CamposBufferizados(Adafruit_ILI9341 &display) : tela(display), imagem(320, 108) {}
  bool memoriaDisponivel() const { return imagem.getBuffer() != nullptr; }
  void reiniciar() { for (auto &anterior : campos) anterior = {}; }

  void campo(uint8_t indice, int16_t x, int16_t y, int16_t largura,
             int16_t altura, const String &texto, uint16_t cor = ILI9341_WHITE,
             uint16_t fundo = ILI9341_BLACK, const GFXfont *fonte = nullptr,
             uint8_t tamanho = 1) {
    if (indice >= QUANTIDADE_CAMPOS) return;
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
    while (tamanho > 1 && (textoLargura > largura || textoAltura > altura)) {
      imagem.setTextSize(--tamanho);
      imagem.getTextBounds(texto, 0, 0, &origemX, &origemY, &textoLargura, &textoAltura);
    }
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
    if (direita > esquerda && base > topo) {
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
    }
    anterior = {texto, cor, fundo, novoX, novoY,
                static_cast<int16_t>(textoLargura), static_cast<int16_t>(textoAltura), true};
  }

 private:
  static constexpr uint8_t QUANTIDADE_CAMPOS = 21;
  struct CampoAnterior {
    String texto;
    uint16_t cor = 0, fundo = 0;
    int16_t x = 0, y = 0, largura = 0, altura = 0;
    bool valido = false;
  } campos[QUANTIDADE_CAMPOS];
  Adafruit_ILI9341 &tela;
  GFXcanvas1 imagem;
  uint16_t linha[320]{};
};
}  // namespace InterfaceDashboard
