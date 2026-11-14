#pragma once

#include "dashboard_f1.h"
#include "perfis_dashboard.h"

namespace DemonstracaoDashboard {
constexpr uint32_t ESPERA_SEM_JOGO_MS = 10000;
constexpr uint32_t INTERVALO_AMOSTRA_MS = 50;
constexpr uint32_t DURACAO_CICLO_MS = 45000;

enum class Etapa : uint8_t {
  Acelerando, Freando, Amarela, Verde, DrsDisponivel, DrsAberto, Azul, Temperaturas
};

constexpr Etapa etapaNoTempo(uint32_t tempo) {
  const uint32_t t = tempo % DURACAO_CICLO_MS;
  return t < 8000 ? Etapa::Acelerando : t < 12000 ? Etapa::Freando :
         t < 16000 ? Etapa::Amarela : t < 20000 ? Etapa::Verde :
         t < 23000 ? Etapa::DrsDisponivel : t < 28000 ? Etapa::DrsAberto :
         t < 33000 ? Etapa::Azul : Etapa::Temperaturas;
}

constexpr const char *nomeEtapa(Etapa etapa) {
  switch (etapa) {
    case Etapa::Acelerando: return "ACELERANDO";
    case Etapa::Freando: return "FREANDO";
    case Etapa::Amarela: return "BANDEIRA AMARELA";
    case Etapa::Verde: return "BANDEIRA VERDE";
    case Etapa::DrsDisponivel: return "DRS DISPONIVEL";
    case Etapa::DrsAberto: return "DRS ABERTO";
    case Etapa::Azul: return "BANDEIRA AZUL";
    default: return "PNEUS E FREIOS";
  }
}

// Pacotes locais passam pelo parser existente. Nenhum pacote e enviado a rede;
// esta telemetria nunca se mistura ao objeto que recebe os dados reais do F1.
class Demonstracao {
 public:
  void aguardar(uint32_t agora) { ultimoDadoReal = agora; }

  bool registrarTelemetriaReal(uint32_t agora) {
    ultimoDadoReal = agora;
    const bool estavaAtiva = emExecucao;
    emExecucao = false;
    if (estavaAtiva) telemetria.reiniciar();
    return estavaAtiva;
  }

  // Retorna true apenas ao iniciar a demonstracao, inclusive apos uma queda.
  bool atualizar(uint32_t agora) {
    bool iniciou = false;
    if (!emExecucao) {
      if (agora - ultimoDadoReal < ESPERA_SEM_JOGO_MS) return false;
      emExecucao = iniciou = true;
      inicio = agora;
      ultimoQuadro = 0;
      ultimaAmostra = agora - INTERVALO_AMOSTRA_MS;
      telemetria.reiniciar();
    }
    if (agora - ultimaAmostra < INTERVALO_AMOSTRA_MS) return iniciou;
    ultimaAmostra = agora;
    tempo = agora - inicio;
    ++ultimoQuadro;
    gerarAmostra(agora);
    return iniciou;
  }

  bool ativa() const { return emExecucao; }
  const DashboardF1::Telemetria &dados() const { return telemetria; }
  Etapa etapa() const { return etapaNoTempo(tempo); }
  PerfisDashboard::Pagina pagina() const {
    return etapa() == Etapa::Temperaturas ? PerfisDashboard::Pagina::Temperaturas :
                                         PerfisDashboard::Pagina::Principal;
  }

 private:
  bool emExecucao = false;
  uint32_t ultimoDadoReal = 0, inicio = 0, tempo = 0;
  uint32_t ultimaAmostra = 0, ultimoQuadro = 0;
  DashboardF1::Telemetria telemetria;
  uint8_t pacote[1460]{};

  void gravar16(size_t posicao, uint16_t valor) {
    pacote[posicao] = valor & 255;
    pacote[posicao + 1] = valor >> 8;
  }
  void gravar32(size_t posicao, uint32_t valor) {
    for (uint8_t i = 0; i < 4; ++i) pacote[posicao + i] = valor >> (i * 8);
  }
  void gravarFloat(size_t posicao, float valor) {
    uint32_t bits;
    std::memcpy(&bits, &valor, sizeof(bits));
    gravar32(posicao, bits);
  }
  void preparar(uint8_t tipo) {
    std::memset(pacote, 0, sizeof(pacote));
    gravar16(0, F1::FORMATO);
    pacote[2] = 25; pacote[5] = 1; pacote[6] = tipo;
    // Identificacao exclusiva da demonstracao; nunca usada pelo receptor UDP.
    pacote[7] = 'D'; pacote[8] = 'E'; pacote[9] = 'M'; pacote[10] = 'O';
    gravar32(23, ultimoQuadro);
    pacote[28] = 255;
  }
  void aplicar(size_t tamanho, uint32_t agora) {
    telemetria.processar(pacote, tamanho, agora);
  }

  void gerarAmostra(uint32_t agora) {
    const uint32_t t = tempo % DURACAO_CICLO_MS;
    const Etapa atual = etapa();
    uint16_t velocidade = 85;
    switch (atual) {
      case Etapa::Acelerando: velocidade = 60 + t * 240 / 8000; break;
      case Etapa::Freando: velocidade = 300 - (t - 8000) * 215 / 4000; break;
      case Etapa::Verde: velocidade = 85 + (t - 16000) * 125 / 4000; break;
      case Etapa::DrsDisponivel: velocidade = 210 + (t - 20000) * 40 / 3000; break;
      case Etapa::DrsAberto: velocidade = 250 + (t - 23000) * 80 / 5000; break;
      case Etapa::Azul: velocidade = 330 - (t - 28000) * 130 / 5000; break;
      case Etapa::Temperaturas: velocidade = 180 + (t - 33000) % 4000 / 50; break;
      default: break;
    }
    const uint8_t marcha = velocidade < 80 ? 2 : velocidade < 115 ? 3 :
        velocidade < 155 ? 4 : velocidade < 195 ? 5 : velocidade < 235 ? 6 :
        velocidade < 280 ? 7 : 8;
    const uint16_t rpm = 6500 + (velocidade % 45) * 150;

    preparar(1);
    pacote[32] = 58; gravar16(33, 5000); pacote[35] = 10; pacote[37] = 0;
    aplicar(753, agora);

    preparar(6);
    gravar16(29, velocidade); pacote[29 + 15] = marcha;
    gravarFloat(29 + 2, atual == Etapa::Freando ? 0.0f : 0.95f);
    gravarFloat(29 + 10, atual == Etapa::Freando ? 0.9f : 0.0f);
    gravar16(29 + 16, rpm);
    pacote[29 + 18] = atual == Etapa::DrsAberto;
    pacote[29 + 19] = (rpm - 6500) * 100 / 6600;
    // Durante a tela de pneus, atravessar todas as faixas de cor.
    const uint32_t aquecimento = atual == Etapa::Temperaturas ? (t - 33000) % 6000 : 3000;
    for (uint8_t roda = 0; roda < 4; ++roda) {
      gravar16(29 + 22 + roda * 2,
               atual == Etapa::Freando ? 1080 - (t - 8000) / 8 + roda * 15 :
                                       200 + aquecimento * 850 / 6000 + roda * 25);
      pacote[29 + 30 + roda] = 65 + aquecimento * 60 / 6000 + roda * 3;
      pacote[29 + 34 + roda] = 60 + aquecimento * 60 / 6000 + roda * 2;
    }
    aplicar(F1::TAMANHO_TELEMETRIA, agora);

    preparar(7);
    pacote[29 + 22] = atual == Etapa::DrsDisponivel || atual == Etapa::DrsAberto;
    pacote[29 + 28] = atual == Etapa::Amarela ? 3 : atual == Etapa::Verde ? 1 :
                         atual == Etapa::Azul ? 2 : 0;
    gravarFloat(29 + 37, 3800000.0f - (t % 20000) * 130.0f);
    pacote[29 + 41] = atual == Etapa::Amarela ? 0 : atual == Etapa::DrsAberto ? 3 :
                         atual == Etapa::Acelerando ? 2 : 1;
    gravarFloat(29 + 50, (tempo % 30000) * (4000000.0f / 30000));
    aplicar(1239, agora);

    preparar(2);
    // Uma volta virtual de 90 s dura 30 s reais e alimenta tambem o delta.
    const uint32_t numero = tempo / 30000;
    const uint32_t parcial = tempo % 30000;
    const uint32_t duracao = numero % 3 == 1 ? 89400 : numero % 3 == 2 ? 90600 : 90000;
    const uint32_t anterior = numero == 0 ? 90000 :
        (numero - 1) % 3 == 1 ? 89400 : (numero - 1) % 3 == 2 ? 90600 : 90000;
    const uint32_t tempoVolta = parcial * duracao / 30000;
    for (uint8_t carro = 0; carro < 3; ++carro) {
      const size_t base = 29 + carro * 57;
      gravar32(base, anterior); gravar32(base + 4, tempoVolta);
      gravar16(base + 14, carro == 0 ? 842 : 1276);
      gravarFloat(base + 20, parcial * (5000.0f / 30000));
      gravarFloat(base + 24, numero * 5000.0f + parcial * (5000.0f / 30000) +
                                   (carro == 1 ? 45 : carro == 2 ? -65 : 0));
      pacote[base + 32] = carro == 0 ? 2 : carro == 1 ? 1 : 3;
      pacote[base + 33] = 12 + numero % 40;
      pacote[base + 44] = 4; pacote[base + 45] = 2;
    }
    aplicar(1285, agora);

    // Nomes e melhores voltas, sem depender de um jogo conectado.
    if (ultimoQuadro == 1 || ultimoQuadro % 20 == 0) {
      preparar(4); pacote[29] = 3;
      constexpr const char *nomes[] = {"Piloto Demo", "Lando Norris", "Charles Leclerc"};
      for (uint8_t carro = 0; carro < 3; ++carro) {
        const size_t base = 30 + carro * 57;
        pacote[base] = carro != 0;
        std::memcpy(pacote + base + 7, nomes[carro], std::strlen(nomes[carro]));
      }
      aplicar(1284, agora);
      for (uint8_t carro = 0; carro < 3; ++carro) {
        preparar(11); pacote[29] = carro; pacote[30] = pacote[32] = 1;
        gravar32(36, carro == 1 ? 89500 : 90000); pacote[49] = 1;
        aplicar(1460, agora);
      }
    }
  }
};
}  // namespace DemonstracaoDashboard
