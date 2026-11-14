#pragma once
#include <stdint.h>

namespace InterfaceDashboard {
struct Estilo { uint16_t texto, fundo; };

constexpr Estilo estiloErs(bool valido, uint8_t modo) {
  if (!valido) return {0xC618, 0x4208};
  switch (modo) {
    case 0: return {0xFFFF, 0x4208};
    case 1: return {0xFFFF, 0x001F};
    case 2: return {0x0000, 0xFFE0};
    case 3: return {0xFFFF, 0xF800};
    default: return {0xC618, 0x4208};
  }
}

// Faixas visuais iniciais, ajustaveis; nao representam um alvo universal de composto.
constexpr uint16_t corTemperaturaPneu(uint8_t celsius) {
  return celsius < 70 ? 0x041F : celsius < 85 ? 0x07FF :
         celsius < 105 ? 0x07E0 : celsius < 115 ? 0xFFE0 : 0xF800;
}
constexpr uint16_t corTemperaturaFreio(uint16_t celsius) {
  return celsius < 300 ? 0x041F : celsius < 500 ? 0x07FF :
         celsius < 850 ? 0x07E0 : celsius < 1000 ? 0xFFE0 : 0xF800;
}
constexpr uint16_t contrasteTemperatura(uint16_t fundo) {
  return fundo == 0x041F || fundo == 0xF800 ? 0xFFFF : 0x0000;
}
}  // namespace InterfaceDashboard
