#pragma once

#include <stdint.h>

namespace AdafruitIO {
constexpr uint32_t INTERVALO_ENVIO_MS = 10000;
constexpr uint32_t INTERVALO_RECONEXAO_MS = 10000;
constexpr uint32_t PAUSA_LIMITE_MS = 60000;
constexpr uint32_t VALIDADE_AMOSTRA_MS = 2000;

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
}  // namespace AdafruitIO
