#pragma once

#include <array>
#include <stddef.h>
#include <stdint.h>

namespace Indicadores {
constexpr size_t QUANTIDADE_RPM = 10;
constexpr uint32_t TEMPO_FASE_BANDEIRA_MS = 500;
// Mesma ordem dos LEDs fisicos: 2 verdes, 2 amarelos, 3 vermelhos, 3 azuis.
constexpr std::array<uint16_t, QUANTIDADE_RPM> CORES_RPM = {
    0x07E0, 0x07E0, 0xFFE0, 0xFFE0, 0xF800, 0xF800, 0xF800, 0x041F, 0x041F, 0x041F};
enum Canal : size_t {
  DRS_DISPONIVEL = QUANTIDADE_RPM,
  DRS_ABERTO,
  BANDEIRA_AMARELA,
  BANDEIRA_VERDE,
  QUANTIDADE_CANAIS
};

constexpr std::array<uint8_t, QUANTIDADE_CANAIS> PINOS = {
    13, 14, 16, 17, 25, 27, 26, 4, 19, 21, 5, 15, 2, 12};
using Estado = std::array<bool, QUANTIDADE_CANAIS>;

constexpr size_t quantidadeAcesa(int percentual) {
  return percentual <= 0 ? 0 : percentual >= 100 ? QUANTIDADE_RPM :
      (percentual * QUANTIDADE_RPM + 99) / 100;
}

constexpr Estado calcularEstado(int percentualRpm, bool drsDisponivel,
                                bool drsAberto, int8_t bandeiraFia, uint32_t agora) {
  Estado estado{};
  for (size_t indice = 0; indice < QUANTIDADE_RPM; ++indice) {
    estado[indice] = indice < quantidadeAcesa(percentualRpm);
  }
  estado[DRS_DISPONIVEL] = drsDisponivel || drsAberto;
  estado[DRS_ABERTO] = drsAberto;
  const bool faseAcesa = (agora / TEMPO_FASE_BANDEIRA_MS) % 2 == 0;
  // Bandeira azul tem prioridade sobre os estados do DRS nos dois brancos.
  if (bandeiraFia == 2) {
    estado[DRS_DISPONIVEL] = estado[DRS_ABERTO] = faseAcesa;
  }
  estado[BANDEIRA_AMARELA] = bandeiraFia == 3 && faseAcesa;
  estado[BANDEIRA_VERDE] = bandeiraFia == 1 && faseAcesa;
  return estado;
}
}  // namespace Indicadores
