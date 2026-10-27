#pragma once

#include <stddef.h>
#include <stdint.h>

namespace F1 {
constexpr uint16_t FORMATO = 2025;
constexpr size_t TAMANHO_CABECALHO = 29;
constexpr size_t TAMANHO_CARRO = 60;
constexpr uint8_t QUANTIDADE_CARROS = 22;
constexpr size_t TAMANHO_TELEMETRIA = 1352;

enum class Resultado {
  Telemetria,
  OutroTipo,
  FormatoIncompativel,
  VersaoIncompativel,
  TamanhoInvalido,
  IndiceInvalido,
  ValorInvalido
};

struct DadosCarro {
  uint16_t velocidadeKmh = 0;
  uint16_t rpm = 0;
  int8_t marcha = 0;
  uint8_t luzesRpm = 0;
  uint8_t indiceCarro = 0;
  uint64_t sessao = 0;
  uint32_t quadro = 0;
};

struct PacoteInterpretado {
  Resultado resultado;
  DadosCarro dados;
};

constexpr uint16_t ler16(const uint8_t *dados) {
  return static_cast<uint16_t>(dados[0]) |
         (static_cast<uint16_t>(dados[1]) << 8);
}

constexpr uint32_t ler32(const uint8_t *dados) {
  uint32_t valor = 0;
  for (uint8_t indice = 0; indice < 4; ++indice) {
    valor |= static_cast<uint32_t>(dados[indice]) << (8 * indice);
  }
  return valor;
}

constexpr uint64_t ler64(const uint8_t *dados) {
  uint64_t valor = 0;
  for (uint8_t indice = 0; indice < 8; ++indice) {
    valor |= static_cast<uint64_t>(dados[indice]) << (8 * indice);
  }
  return valor;
}

// Layout packed e little-endian da especificacao oficial F1 25, pacote 6 v1.
constexpr PacoteInterpretado interpretar(const uint8_t *pacote, size_t tamanho) {
  if (pacote == nullptr || tamanho < TAMANHO_CABECALHO) {
    return {Resultado::TamanhoInvalido, {}};
  }
  if (ler16(pacote) != FORMATO) {
    return {Resultado::FormatoIncompativel, {}};
  }
  if (pacote[6] > 15) {
    return {Resultado::ValorInvalido, {}};
  }
  if (pacote[6] != 6) {
    return {Resultado::OutroTipo, {}};
  }
  if (pacote[5] != 1) {
    return {Resultado::VersaoIncompativel, {}};
  }
  if (tamanho != TAMANHO_TELEMETRIA) {
    return {Resultado::TamanhoInvalido, {}};
  }
  const uint8_t indiceCarro = pacote[27];
  if (indiceCarro >= QUANTIDADE_CARROS) {
    return {Resultado::IndiceInvalido, {}};
  }

  const uint8_t *carro = pacote + TAMANHO_CABECALHO + indiceCarro * TAMANHO_CARRO;
  const int16_t marcha = carro[15] < 128 ? carro[15] : carro[15] - 256;
  if (marcha < -1 || marcha > 8 || carro[19] > 100) {
    return {Resultado::ValorInvalido, {}};
  }

  return {Resultado::Telemetria,
          {ler16(carro), ler16(carro + 16), static_cast<int8_t>(marcha),
           carro[19], indiceCarro, ler64(pacote + 7), ler32(pacote + 23)}};
}

constexpr bool quadroMaisRecente(uint32_t novo, uint32_t anterior) {
  const uint32_t diferenca = novo - anterior;
  return diferenca != 0 && diferenca < 0x80000000UL;
}
}  // namespace F1
