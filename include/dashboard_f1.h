#pragma once

#include <cmath>
#include <cstring>
#include "telemetria_f1.h"
#include "delta_ultima_volta.h"

namespace DashboardF1 {
constexpr uint32_t VALIDADE_DADOS = 3000;
constexpr uint32_t VALIDADE_HISTORICO = 10000;
constexpr uint32_t BOTAO_ACAO_UDP_1 = 0x00100000UL;

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
  uint32_t anterior = 0;
  DeltaVolta delta;
  uint8_t indiceFrente = 255, indiceAtras = 255;
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
  int8_t bandeiraFia = -1;
  bool jogoPausado = false;
  bool carroPausadoNaRede = false;

  void reiniciar() {
    carro = {}; volta = {}; energia = {};
    totalVoltas = tipoSessao = 0; formula = 255; comprimentoPista = 0;
    bandeiraFia = -1; jogoPausado = carroPausadoNaRede = false;
    haIdentificacao = haSessaoAnterior = false;
    sessao = sessaoAnterior = 0; jogador = 0;
    haCarro = haVolta = haEnergia = haSessao = false;
    ultimoCarro = ultimaVolta = ultimaEnergia = ultimaSessao = 0;
    std::memset(quadrosTipo, 0, sizeof(quadrosTipo));
    std::memset(haQuadroTipo, 0, sizeof(haQuadroTipo));
    std::memset(quadrosHistorico, 0, sizeof(quadrosHistorico));
    std::memset(haQuadroHistorico, 0, sizeof(haQuadroHistorico));
    std::memset(melhores, 0, sizeof(melhores));
    std::memset(ultimosHistoricos, 0, sizeof(ultimosHistoricos));
    std::memset(haHistorico, 0, sizeof(haHistorico));
    std::memset(nomes, 0, sizeof(nomes));
    std::memset(controladoIa, 0, sizeof(controladoIa));
    quantidadeParticipantes = 0;
    estadoBotoes = 0; acaoUdp1Pendente = false;
    haQuadroBotoes = false; quadroBotoes = 0;
    limitarHistorico = false; inicioHistorico = 0;
    referenciaDelta.reiniciar();
  }

  const char *nomePiloto(uint8_t indice) const {
    return indice < quantidadeParticipantes ? nomes[indice] : "";
  }
  bool pilotoIa(uint8_t indice) const {
    return indice < quantidadeParticipantes && controladoIa[indice];
  }
  bool consumirAcaoUdp1() {
    const bool pendente = acaoUdp1Pendente;
    acaoUdp1Pendente = false;
    return pendente;
  }

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
  bool pausado(uint32_t agora) const {
    return (sessaoRecente(agora) && jogoPausado) ||
           (energiaRecente(agora) && carroPausadoNaRede);
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
      case 4: esperado = 1284; break;
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
    const bool botoes = tipo == 3 && std::memcmp(pacote + 29, "BUTN", 4) == 0;
    if (tipo == 3 && !flashback && !botoes) return Resultado::Ignorado;
    if (tipo == 4) {
      if (pacote[29] > F1::QUANTIDADE_CARROS) return Resultado::ValorInvalido;
      for (uint8_t indice = 0; indice < pacote[29]; ++indice) {
        const uint8_t *participante = pacote + 30 + indice * 57;
        if (participante[0] > 1 || participante[44] > 4) return Resultado::ValorInvalido;
      }
    }

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
          estado[41] > 3 || estado[22] > 1 ||
          (estado[28] != 255 && estado[28] > 3) || estado[54] > 1) {
        return Resultado::ValorInvalido;
      }
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
    bool &haQuadro = botoes ? haQuadroBotoes : tipo == 11 ? haQuadroHistorico[indiceHistorico] : haQuadroTipo[tipo];
    uint32_t &ultimoQuadro = botoes ? quadroBotoes : tipo == 11 ? quadrosHistorico[indiceHistorico] : quadrosTipo[tipo];
    // Dois eventos podem ocorrer no mesmo quadro. Aceitar mudanca dos botoes,
    // preservando a ordenacao independente dos flashbacks e demais pacotes.
    const bool mudancaNoMesmoQuadro = botoes && quadro == ultimoQuadro &&
                                     F1::ler32(pacote + 33) != estadoBotoes;
    if (haQuadro && !F1::quadroMaisRecente(quadro, ultimoQuadro) && !mudancaNoMesmoQuadro) {
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
        jogoPausado = pacote[43] == 1;
        haSessao = true;
        ultimaSessao = agora;
        break;
      case 2:
        lerVoltas(pacote, quadro);
        haVolta = true;
        ultimaVolta = agora;
        break;
      case 3:
        if (botoes) {
          const uint32_t novoEstado = F1::ler32(pacote + 33);
          if ((novoEstado & BOTAO_ACAO_UDP_1) && !(estadoBotoes & BOTAO_ACAO_UDP_1))
            acaoUdp1Pendente = true;
          estadoBotoes = novoEstado;
          break;
        }
        limparHistoricos(quadro);
        referenciaDelta.reiniciar();
        volta.delta = {};
        haVolta = false;
        haEnergia = false;
        haCarro = false;
        break;
      case 4:
        quantidadeParticipantes = pacote[29];
        std::memset(nomes, 0, sizeof(nomes));
        std::memset(controladoIa, 0, sizeof(controladoIa));
        for (uint8_t indice = 0; indice < quantidadeParticipantes; ++indice) {
          const uint8_t *participante = pacote + 30 + indice * 57;
          std::memcpy(nomes[indice], participante + 7, 32);
          controladoIa[indice] = participante[0] == 1;
        }
        break;
      case 6:
        carro = dadosCarro.dados;
        haCarro = true;
        ultimoCarro = agora;
        break;
      case 7:
        energia = {lerFloat(estado + 37), lerFloat(estado + 50), estado[41], estado[22] == 1};
        bandeiraFia = estado[28] == 255 ? -1 : static_cast<int8_t>(estado[28]);
        carroPausadoNaRede = estado[54] == 1;
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
  char nomes[F1::QUANTIDADE_CARROS][33]{};
  bool controladoIa[F1::QUANTIDADE_CARROS]{};
  uint8_t quantidadeParticipantes = 0;
  uint32_t estadoBotoes = 0, quadroBotoes = 0;
  bool acaoUdp1Pendente = false, haQuadroBotoes = false;
  DeltaUltimaVolta referenciaDelta;

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
    if (!carroValido(atual)) { referenciaDelta.reiniciar(); return; }
    volta.posicao = atual[32];
    volta.numero = atual[33];
    volta.tempo = F1::ler32(atual + 4);
    volta.anterior = F1::ler32(atual);
    volta.invalida = atual[37] != 0;
    volta.delta = referenciaDelta.atualizar(
        comprimentoPista, volta.numero, lerFloat(atual + 20), volta.tempo, volta.anterior,
        !volta.invalida && atual[34] == 0 && (atual[44] == 1 || atual[44] == 4));
    const uint8_t *frente = nullptr;
    const uint8_t *atras = nullptr;
    for (uint8_t indice = 0; indice < 22; ++indice) {
      const uint8_t *outro = pacote + 29 + indice * 57;
      if (!carroValido(outro)) continue;
      if (outro[32] + 1 == volta.posicao) { frente = outro; volta.indiceFrente = indice; }
      if (outro[32] == volta.posicao + 1) { atras = outro; volta.indiceAtras = indice; }
    }
    volta.frente = lerIntervalo(atual, frente, atual);
    volta.atras = lerIntervalo(atual, atras, atras);
  }
};
}  // namespace DashboardF1
