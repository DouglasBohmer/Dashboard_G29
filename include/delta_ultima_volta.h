#pragma once

#include <cmath>
#include <stddef.h>
#include <stdint.h>

namespace DashboardF1 {
struct DeltaVolta {
  bool valido = false;
  int32_t milissegundos = 0;
};

// Curvas de tempo por distancia: a referencia e sempre a ultima volta completa.
class DeltaUltimaVolta {
 public:
  static constexpr size_t TRECHOS = 512;

  void reiniciar() {
    comprimento = numero = proximo = 0;
    atual = 0;
    iniciado = registrando = haReferencia = false;
    ultimaDistancia = 0;
    ultimoTempo = 0;
  }

  DeltaVolta atualizar(uint16_t pista, uint8_t volta, float distancia,
                      uint32_t tempo, uint32_t anterior, bool valida) {
    if (comprimento != pista) { reiniciar(); comprimento = pista; }
    if (!pista || !volta || !std::isfinite(distancia) || distancia < 0 ||
        distancia > pista || tempo >= 86400000UL) {
      registrando = false;
      return {};
    }
    if (iniciado && (volta < numero || (volta == numero &&
        (tempo < ultimoTempo || distancia < ultimaDistancia)))) {
      reiniciar();
      comprimento = pista;
    }
    if (!iniciado || volta != numero) {
      if (iniciado) {
        haReferencia = volta == numero + 1 && registrando && anterior > 0 &&
                       anterior < 86400000UL && registrar(pista, anterior) &&
                       proximo == TRECHOS + 1;
        if (haReferencia) atual ^= 1;
      }
      numero = volta;
      proximo = 1;
      tempos[atual][0] = 0;
      ultimoTempo = 0;
      ultimaDistancia = 0;
      iniciado = true;
      registrando = valida && tempo <= 1000 && distancia <= 200;
    }
    if (!valida) registrando = false;
    if (registrando && !registrar(distancia, tempo)) registrando = false;
    ultimaDistancia = distancia;
    ultimoTempo = tempo;
    if (!valida || !haReferencia) return {};

    const float trecho = distancia * TRECHOS / comprimento;
    size_t indice = static_cast<size_t>(trecho);
    if (indice >= TRECHOS) indice = TRECHOS - 1;
    const auto &referencia = tempos[atual ^ 1];
    const float esperado = referencia[indice] +
        (referencia[indice + 1] - referencia[indice]) * (trecho - indice);
    return {true, static_cast<int32_t>(tempo) - static_cast<int32_t>(esperado + 0.5f)};
  }

 private:
  uint32_t tempos[2][TRECHOS + 1]{};
  uint16_t comprimento = 0, proximo = 0;
  uint8_t numero = 0, atual = 0;
  bool iniciado = false, registrando = false, haReferencia = false;
  float ultimaDistancia = 0;
  uint32_t ultimoTempo = 0;

  bool registrar(float distancia, uint32_t tempo) {
    if (distancia < ultimaDistancia || tempo < ultimoTempo ||
        tempo - ultimoTempo > 1000 || distancia - ultimaDistancia > 200) return false;
    if (distancia == ultimaDistancia) return true;
    while (proximo <= TRECHOS) {
      const float ponto = static_cast<float>(proximo) * comprimento / TRECHOS;
      if (ponto > distancia) break;
      const float fracao = (ponto - ultimaDistancia) / (distancia - ultimaDistancia);
      tempos[atual][proximo++] = ultimoTempo +
          static_cast<uint32_t>((tempo - ultimoTempo) * fracao + 0.5f);
    }
    return true;
  }
};
}  // namespace DashboardF1
