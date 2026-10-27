#pragma once

#include <cmath>
#include <cstring>
#include "telemetria_f1.h"

namespace DashboardF1 {
constexpr uint32_t VALIDADE_DADOS = 3000;
constexpr uint32_t VALIDADE_HISTORICO = 10000;

enum class Resultado {
  Atualizado, Ignorado, FormatoIncompativel, VersaoIncompativel,
  TamanhoInvalido, IndiceInvalido, ValorInvalido, ForaDeOrdem
};

inline float lerFloat(const uint8_t *dados) {
  const uint32_t bits = F1::ler32(dados);
  float valor;
  static_assert(sizeof(valor) == sizeof(bits), "Float deve ter 32 bits");
  std::memcpy(&valor, &bits, sizeof(valor));
  return valor;
}

inline int percentual(float valor, float limite) {
  if (!std::isfinite(valor) || !std::isfinite(limite) || valor < 0 || limite <= 0) {
    return -1;
  }
  const float proporcao = valor / limite;
  if (proporcao >= 1) return 100;
  return static_cast<int>(proporcao * 100.0f + 0.5f);
}

struct Intervalo {
  bool valido = false;
  uint32_t milissegundos = 0;
  uint8_t voltas = 0;
};

struct DadosVolta {
  uint8_t posicao = 0;
  uint8_t numero = 0;
  bool invalida = false;
  uint32_t tempo = 0;
  Intervalo frente;
  Intervalo atras;
};

struct DadosEnergia {
  float armazenada = 0;
  float aplicadaNaVolta = 0;
  uint8_t modo = 0;
  bool drsPermitido = false;
};

class Telemetria {
 public:
  F1::DadosCarro carro{};
  DadosVolta volta{};
  DadosEnergia energia{};
  uint8_t totalVoltas = 0;
  uint8_t formula = 255;
  uint8_t tipoSessao = 0;
  uint16_t comprimentoPista = 0;

  void reiniciar() { *this = Telemetria{}; }

  bool carroRecente(uint32_t agora) const {
    return haCarro && agora - ultimoCarro < VALIDADE_DADOS;
  }
  bool voltaRecente(uint32_t agora) const {
    return haVolta && agora - ultimaVolta < VALIDADE_DADOS;
  }
  bool energiaRecente(uint32_t agora) const {
    return haEnergia && agora - ultimaEnergia < VALIDADE_DADOS;
  }
  bool sessaoRecente(uint32_t agora) const {
    return haSessao && agora - ultimaSessao < VALIDADE_HISTORICO;
  }
  uint32_t minhaMelhor(uint32_t agora) const {
    return melhorRecente(jogador, agora);
  }
  uint32_t melhorProva(uint32_t agora) const {
    uint32_t melhor = 0;
    for (uint8_t indice = 0; indice < F1::QUANTIDADE_CARROS; ++indice) {
      const uint32_t tempo = melhorRecente(indice, agora);
      if (tempo && (!melhor || tempo < melhor)) melhor = tempo;
    }
    return melhor;
  }

  Resultado processar(const uint8_t *pacote, size_t tamanho, uint32_t agora) {
    if (!pacote || tamanho < F1::TAMANHO_CABECALHO) return Resultado::TamanhoInvalido;
    if (F1::ler16(pacote) != F1::FORMATO) return Resultado::FormatoIncompativel;
    const uint8_t tipo = pacote[6];
    if (tipo > 15) return Resultado::ValorInvalido;
    size_t esperado = 0;
    switch (tipo) {
      case 1: esperado = 753; break;
      case 2: esperado = 1285; break;
      case 3: esperado = 45; break;
      case 6: esperado = F1::TAMANHO_TELEMETRIA; break;
      case 7: esperado = 1239; break;
      case 11: esperado = 1460; break;
      default: return Resultado::Ignorado;
    }
    if (pacote[5] != 1) return Resultado::VersaoIncompativel;
    if (tamanho != esperado) return Resultado::TamanhoInvalido;
    const uint8_t indiceJogador = pacote[27];
    if (indiceJogador >= F1::QUANTIDADE_CARROS) return Resultado::IndiceInvalido;
    const bool flashback = tipo == 3 && std::memcmp(pacote + 29, "FLBK", 4) == 0;
    if (tipo == 3 && !flashback) return Resultado::Ignorado;

    uint8_t indiceHistorico = 0;
    if (tipo == 11) {
      indiceHistorico = pacote[29];
      if (indiceHistorico >= F1::QUANTIDADE_CARROS) return Resultado::IndiceInvalido;
      if (pacote[30] > 100 || pacote[31] > 8 || pacote[32] > pacote[30]) {
        return Resultado::ValorInvalido;
      }
    }
    F1::PacoteInterpretado dadosCarro{};
    if (tipo == 6) {
      dadosCarro = F1::interpretar(pacote, tamanho);
      if (dadosCarro.resultado != F1::Resultado::Telemetria) return Resultado::ValorInvalido;
    }
    const uint8_t *estado = nullptr;
    if (tipo == 7) {
      estado = pacote + 29 + indiceJogador * 55;
      const float carga = lerFloat(estado + 37);
      const float aplicada = lerFloat(estado + 50);
      if (!std::isfinite(carga) || !std::isfinite(aplicada) || carga < 0 ||
          aplicada < 0 || carga > 1.0e9f || aplicada > 1.0e9f ||
          estado[41] > 3 || estado[22] > 1) return Resultado::ValorInvalido;
    }

    const uint64_t novaSessao = F1::ler64(pacote + 7);
    const uint32_t quadro = F1::ler32(pacote + 23);
    if (haIdentificacao && haSessaoAnterior && novaSessao != sessao && novaSessao == sessaoAnterior) {
      return Resultado::ForaDeOrdem;
    }
    if (!haIdentificacao || novaSessao != sessao || jogador != indiceJogador) {
      const uint64_t anterior = sessao;
      const bool haviaIdentificacao = haIdentificacao;
      reiniciar();
      sessaoAnterior = anterior;
      haSessaoAnterior = haviaIdentificacao;
      sessao = novaSessao;
      jogador = indiceJogador;
      haIdentificacao = true;
    }
    bool &haQuadro = tipo == 11 ? haQuadroHistorico[indiceHistorico] : haQuadroTipo[tipo];
    uint32_t &ultimoQuadro = tipo == 11 ? quadrosHistorico[indiceHistorico] : quadrosTipo[tipo];
    if (haQuadro && !F1::quadroMaisRecente(quadro, ultimoQuadro)) {
      return Resultado::ForaDeOrdem;
    }
    if (tipo == 11 && limitarHistorico && quadro != inicioHistorico &&
        !F1::quadroMaisRecente(quadro, inicioHistorico)) return Resultado::ForaDeOrdem;
    haQuadro = true;
    ultimoQuadro = quadro;

    switch (tipo) {
      case 1:
        totalVoltas = pacote[32];
        comprimentoPista = F1::ler16(pacote + 33);
        tipoSessao = pacote[35];
        formula = pacote[37];
        haSessao = true;
        ultimaSessao = agora;
        break;
      case 2:
        lerVoltas(pacote, quadro);
        haVolta = true;
        ultimaVolta = agora;
        break;
      case 3:
        limparHistoricos(quadro);
        haVolta = false;
        haEnergia = false;
        haCarro = false;
        break;
      case 6:
        carro = dadosCarro.dados;
        haCarro = true;
        ultimoCarro = agora;
        break;
      case 7:
        energia = {lerFloat(estado + 37), lerFloat(estado + 50), estado[41], estado[22] == 1};
        haEnergia = true;
        ultimaEnergia = agora;
        break;
      case 11:
        melhores[indiceHistorico] = 0;
        for (uint8_t indice = 0; indice < pacote[30]; ++indice) {
          const uint8_t *registro = pacote + 36 + indice * 14;
          const uint32_t tempo = F1::ler32(registro);
          if ((registro[13] & 1) && tempo &&
              (!melhores[indiceHistorico] || tempo < melhores[indiceHistorico])) {
            melhores[indiceHistorico] = tempo;
          }
        }
        ultimosHistoricos[indiceHistorico] = agora;
        haHistorico[indiceHistorico] = true;
        break;
    }
    return Resultado::Atualizado;
  }

 private:
  bool haIdentificacao = false;
  bool haSessaoAnterior = false;
  uint64_t sessao = 0;
  uint64_t sessaoAnterior = 0;
  uint8_t jogador = 0;
  bool haCarro = false, haVolta = false, haEnergia = false, haSessao = false;
  uint32_t ultimoCarro = 0, ultimaVolta = 0, ultimaEnergia = 0, ultimaSessao = 0;
  uint32_t quadrosTipo[16]{};
  bool haQuadroTipo[16]{};
  uint32_t quadrosHistorico[22]{};
  bool haQuadroHistorico[22]{};
  uint32_t melhores[22]{};
  uint32_t ultimosHistoricos[22]{};
  bool haHistorico[22]{};
  bool limitarHistorico = false;
  uint32_t inicioHistorico = 0;

  uint32_t melhorRecente(uint8_t indice, uint32_t agora) const {
    return haHistorico[indice] && agora - ultimosHistoricos[indice] < VALIDADE_HISTORICO
               ? melhores[indice] : 0;
  }
  void limparHistoricos(uint32_t quadro) {
    for (uint8_t indice = 0; indice < 22; ++indice) {
      haHistorico[indice] = false;
      haQuadroHistorico[indice] = false;
    }
    limitarHistorico = true;
    inicioHistorico = quadro;
  }
  static bool carroValido(const uint8_t *dados) {
    return dados[32] >= 1 && dados[32] <= 22 && (dados[45] == 2 || dados[45] == 3);
  }
  Intervalo lerIntervalo(const uint8_t *origem, const uint8_t *referencia,
                       const uint8_t *portador) const {
    Intervalo intervalo;
    if (!origem || !referencia || !carroValido(origem) || !carroValido(referencia)) {
      return intervalo;
    }
    const float distanciaA = lerFloat(origem + 24);
    const float distanciaB = lerFloat(referencia + 24);
    if (comprimentoPista && std::isfinite(distanciaA) && std::isfinite(distanciaB)) {
      const float diferenca = std::fabs(distanciaA - distanciaB);
      if (diferenca >= comprimentoPista) {
        intervalo.valido = true;
        const float voltas = diferenca / comprimentoPista;
        intervalo.voltas = voltas > 99 ? 99 : static_cast<uint8_t>(voltas);
        return intervalo;
      }
    }
    const uint16_t parteMs = F1::ler16(portador + 14);
    if (parteMs >= 60000) return intervalo;
    intervalo.milissegundos = portador[16] * 60000UL + parteMs;
    intervalo.valido = intervalo.milissegundos > 0;
    return intervalo;
  }
  void lerVoltas(const uint8_t *pacote, uint32_t quadro) {
    const uint8_t *atual = pacote + 29 + jogador * 57;
    if (haVolta && atual[33] < volta.numero) limparHistoricos(quadro);
    volta = {};
    if (!carroValido(atual)) return;
    volta.posicao = atual[32];
    volta.numero = atual[33];
    volta.tempo = F1::ler32(atual + 4);
    volta.invalida = atual[37] != 0;
    const uint8_t *frente = nullptr;
    const uint8_t *atras = nullptr;
    for (uint8_t indice = 0; indice < 22; ++indice) {
      const uint8_t *outro = pacote + 29 + indice * 57;
      if (!carroValido(outro)) continue;
      if (outro[32] + 1 == volta.posicao) frente = outro;
      if (outro[32] == volta.posicao + 1) atras = outro;
    }
    volta.frente = lerIntervalo(atual, frente, atual);
    volta.atras = lerIntervalo(atual, atras, atras);
  }
};
}  // namespace DashboardF1
