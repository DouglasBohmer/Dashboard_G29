#pragma once

#include <stdint.h>

namespace ConfiguracaoLuminosidade {
constexpr uint8_t PINO_SENSOR = 34;
constexpr bool NIVEL_ALTO_INDICA_ESCURO = true;
constexpr uint32_t INTERVALO_LEITURA_MS = 20;
constexpr uint32_t TEMPO_CONFIRMACAO_MS = 300;
constexpr uint8_t BRILHO_ESCURO = 90;
constexpr uint8_t BRILHO_CLARO = 255;
constexpr uint8_t CANAL_PWM = 0;
constexpr uint32_t FREQUENCIA_PWM_HZ = 5000;
constexpr uint8_t RESOLUCAO_PWM_BITS = 8;
constexpr uint32_t INTERVALO_AJUSTE_MS = 20;
constexpr uint8_t PASSO_BRILHO = 5;
}  // namespace ConfiguracaoLuminosidade

namespace Luminosidade {
class Sensor {
 public:
  constexpr void atualizar(bool nivelAlto, uint32_t agora) {
    if (!iniciado || nivelAlto != candidato) {
      iniciado = true;
      candidato = nivelAlto;
      inicioCandidato = agora;
      return;
    }
    if (agora - inicioCandidato >= ConfiguracaoLuminosidade::TEMPO_CONFIRMACAO_MS) {
      confirmado = candidato;
      leituraValida = true;
    }
  }

  constexpr bool valido() const { return leituraValida; }
  constexpr bool escuro() const {
    return leituraValida && confirmado == ConfiguracaoLuminosidade::NIVEL_ALTO_INDICA_ESCURO;
  }
  constexpr uint8_t brilhoAlvo() const {
    return escuro() ? ConfiguracaoLuminosidade::BRILHO_ESCURO
                    : ConfiguracaoLuminosidade::BRILHO_CLARO;
  }

 private:
  bool iniciado = false, leituraValida = false;
  bool candidato = false, confirmado = false;
  uint32_t inicioCandidato = 0;
};

constexpr uint8_t aproximarBrilho(uint8_t atual, uint8_t alvo) {
  const uint8_t passo = ConfiguracaoLuminosidade::PASSO_BRILHO;
  if (atual < alvo) return alvo - atual <= passo ? alvo : atual + passo;
  if (atual > alvo) return atual - alvo <= passo ? alvo : atual - passo;
  return atual;
}
}  // namespace Luminosidade
