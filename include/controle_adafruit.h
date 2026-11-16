#pragma once

#include <stdint.h>

namespace AdafruitIO {
constexpr uint32_t INTERVALO_ENVIO_MS = 10000;
constexpr uint32_t INTERVALO_RECONEXAO_MS = 10000;
constexpr uint32_t PAUSA_LIMITE_MS = 60000;
constexpr uint32_t VALIDADE_AMOSTRA_MS = 2000;
// LDR, brilho e umidade: tres pontos a cada envio.
constexpr unsigned PONTOS_POR_ENVIO = 3;

class Temporizador {
 public:
  constexpr bool consumir(uint32_t agora, uint32_t intervalo) {
    if (iniciado && agora - anterior < intervalo) return false;
    iniciado = true;
    anterior = agora;
    return true;
  }

 private:
  bool iniciado = false;
  uint32_t anterior = 0;
};

constexpr uint8_t percentualBrilho(uint8_t pwm) {
  return (pwm * 100U + 127U) / 255U;
}

// Umidade relativa publicada somente quando finita e entre 0 e 100%.
constexpr bool umidadePublicavel(float umidade) {
  return umidade == umidade && umidade >= 0.0f && umidade <= 100.0f;
}
}  // namespace AdafruitIO
