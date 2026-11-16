#pragma once
#include <array>
#include <stddef.h>
#include <stdint.h>

namespace DiagnosticoTFT {
using Leitura = std::array<uint8_t, 8>;  // ID4, modo, pixel, MADCTL, autodiagnostico.
constexpr bool contem9341(const uint8_t *dados, size_t tamanho) {
  for (size_t i = 1; i < tamanho; ++i)
    if (dados[i - 1] == 0x93 && dados[i] == 0x41) return true;
  return false;
}
constexpr bool uniforme(const Leitura &dados, uint8_t valor) {
  for (uint8_t dado : dados) if (dado != valor) return false;
  return true;
}
struct Resumo {
  uint16_t amostras = 0, compativeis = 0, zeros = 0, altos = 0;
  constexpr void registrar(const Leitura &leitura) {
    ++amostras;
    compativeis += contem9341(leitura.data(), 4);
    zeros += uniforme(leitura, 0);
    altos += uniforme(leitura, 0xFF);
  }
};
}  // namespace DiagnosticoTFT
