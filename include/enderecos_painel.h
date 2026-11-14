#pragma once

#include <array>
#include <stdint.h>

namespace RedePainel {
using IPv4 = std::array<uint8_t, 4>;
constexpr IPv4 PREFERIDO = {192, 168, 8, 80};
constexpr uint8_t QUANTIDADE_OPCOES = 5;
constexpr uint16_t PORTA_UDP = 20777;

constexpr uint32_t inteiro(const IPv4 &ip) {
  return (uint32_t(ip[0]) << 24) | (uint32_t(ip[1]) << 16) |
         (uint32_t(ip[2]) << 8) | ip[3];
}
constexpr IPv4 octetos(uint32_t valor) {
  return {uint8_t(valor >> 24), uint8_t(valor >> 16), uint8_t(valor >> 8), uint8_t(valor)};
}
constexpr bool utilizavel(const IPv4 &ip, const IPv4 &origem, const IPv4 &mascara) {
  const uint32_t m = inteiro(mascara), endereco = inteiro(ip), rede = inteiro(origem) & m;
  return m && (endereco & m) == rede && endereco != rede && endereco != (rede | ~m);
}
constexpr IPv4 candidato(const IPv4 &origem, const IPv4 &mascara, uint8_t opcao) {
  if (opcao >= QUANTIDADE_OPCOES) return {};
  auto preferido = PREFERIDO;
  preferido[3] += opcao;
  if (utilizavel(preferido, origem, mascara)) return preferido;
  const uint32_t host = 80U + opcao, m = inteiro(mascara);
  if (!m || host >= ~m) return {};
  return octetos((inteiro(origem) & m) | host);
}
}  // namespace RedePainel
