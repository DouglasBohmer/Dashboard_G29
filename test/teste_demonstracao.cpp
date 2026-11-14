#include <cstdio>
#include <cstring>
#include "demonstracao_dashboard.h"
#include "indicadores_painel.h"
#include "cores_dashboard.h"

namespace {
unsigned aprovadas = 0, falhas = 0;
void exigir(bool condicao, const char *descricao) {
  if (condicao) ++aprovadas;
  else { ++falhas; std::printf("FALHA: %s\n", descricao); }
}

void testarCiclo() {
  DemonstracaoDashboard::Demonstracao demo;
  demo.aguardar(500);
  exigir(!demo.atualizar(10499) && !demo.ativa(), "aguarda dez segundos completos sem jogo");
  exigir(demo.atualizar(10500) && demo.ativa(), "inicia exatamente apos dez segundos");
  const uint32_t inicio = 10500;
  uint16_t velocidadeAnterior = demo.dados().carro.velocidadeKmh;
  bool aceleracao = true, frenagem = true, recentes = true;
  bool viuAmarela = false, viuVerde = false, viuAzul = false;
  bool viuDrsDisponivel = false, viuDrsAberto = false, viuRpmCompleto = false;
  bool viuDeltaGanho = false, viuDeltaPerda = false;
  bool coresPneu[5]{}, coresFreio[5]{};
  constexpr uint16_t cores[] = {0x041F, 0x07FF, 0x07E0, 0xFFE0, 0xF800};
  for (uint32_t t = 50; t <= 90000; t += 50) {
    const uint32_t agora = inicio + t;
    exigir(!demo.atualizar(agora), "ciclo em execucao nao anuncia um novo inicio");
    const auto &dados = demo.dados();
    if (t == 8000) exigir(demo.etapa() == DemonstracaoDashboard::Etapa::Freando, "aceleracao seguida de frenagem");
    if (t == 12000) exigir(dados.bandeiraFia == 3, "amarela depois da frenagem");
    if (t == 16000) exigir(dados.bandeiraFia == 1, "verde depois da amarela");
    if (t == 20000) exigir(dados.energia.drsPermitido && !dados.carro.drsAtivo, "DRS disponivel depois da verde");
    if (t == 23000) exigir(dados.carro.drsAtivo, "DRS abre depois de ficar disponivel");
    if (t == 28000) exigir(dados.bandeiraFia == 2 && !dados.carro.drsAtivo, "azul depois do DRS");
    recentes &= dados.carroRecente(agora) && dados.voltaRecente(agora) &&
                dados.energiaRecente(agora) && dados.sessaoRecente(agora) && !dados.pausado(agora);
    if (t < 8000) aceleracao &= dados.carro.velocidadeKmh >= velocidadeAnterior;
    if (t >= 8000 && t < 12000) frenagem &= dados.carro.velocidadeKmh <= velocidadeAnterior + 2;
    velocidadeAnterior = dados.carro.velocidadeKmh;
    const auto leds = Indicadores::calcularEstado(dados.carro.luzesRpm, dados.energia.drsPermitido,
                                                dados.carro.drsAtivo, dados.bandeiraFia, agora);
    viuRpmCompleto |= leds[Indicadores::QUANTIDADE_RPM - 1];
    viuAmarela |= dados.bandeiraFia == 3 && leds[Indicadores::BANDEIRA_AMARELA];
    viuVerde |= dados.bandeiraFia == 1 && leds[Indicadores::BANDEIRA_VERDE];
    viuAzul |= dados.bandeiraFia == 2 && leds[Indicadores::DRS_ABERTO] && leds[Indicadores::DRS_DISPONIVEL];
    viuDrsDisponivel |= dados.energia.drsPermitido && !dados.carro.drsAtivo;
    viuDrsAberto |= dados.carro.drsAtivo && leds[Indicadores::DRS_ABERTO];
    viuDeltaGanho |= dados.volta.delta.valido && dados.volta.delta.milissegundos < 0;
    viuDeltaPerda |= dados.volta.delta.valido && dados.volta.delta.milissegundos > 0;
    if (t >= 33000 && t < 45000) {
      exigir(demo.pagina() == PerfisDashboard::Pagina::Temperaturas, "troca automatica para pneus apos a bandeira azul");
      for (uint8_t roda = 0; roda < 4; ++roda) {
        for (uint8_t cor = 0; cor < 5; ++cor) {
          coresPneu[cor] |= InterfaceDashboard::corTemperaturaPneu(dados.carro.temperaturaPneusInterna[roda]) == cores[cor];
          coresFreio[cor] |= InterfaceDashboard::corTemperaturaFreio(dados.carro.temperaturaFreios[roda]) == cores[cor];
        }
      }
    }
    if (t == 45000) exigir(demo.pagina() == PerfisDashboard::Pagina::Principal && dados.carro.velocidadeKmh == 60,
                            "repete o ciclo e retorna para a principal");
  }
  exigir(aceleracao && frenagem, "velocidade sobe ao acelerar e cai ao frear");
  exigir(recentes, "todos os pacotes simulados sao aceitos e mantem validade");
  exigir(viuAmarela && viuVerde && viuAzul, "todas as bandeiras acionam os LEDs existentes");
  exigir(viuDrsDisponivel && viuDrsAberto && viuRpmCompleto, "DRS e dez segmentos RPM sao exercitados");
  exigir(viuDeltaGanho && viuDeltaPerda, "delta compara voltas completas com ganho e perda");
  for (uint8_t cor = 0; cor < 5; ++cor)
    exigir(coresPneu[cor] && coresFreio[cor], "temperaturas percorrem todas as cinco faixas de cor");
  exigir(demo.dados().volta.posicao == 2 && demo.dados().volta.frente.valido &&
         demo.dados().volta.atras.valido, "posicao e intervalos dos dois vizinhos disponiveis");
  exigir(std::strcmp(demo.dados().nomePiloto(1), "Lando Norris") == 0 && demo.dados().pilotoIa(1),
         "participantes alimentam os nomes dos vizinhos");
  exigir(demo.dados().minhaMelhor(inicio + 90000) > 0 && demo.dados().melhorProva(inicio + 90000) > 0,
         "historicos simulados permanecem recentes");
}

void testarPrioridadeReal() {
  DemonstracaoDashboard::Demonstracao demo;
  DashboardF1::Telemetria real;
  uint8_t pacote[F1::TAMANHO_TELEMETRIA]{};
  pacote[0] = 0xE9; pacote[1] = 0x07; pacote[5] = 1; pacote[6] = 6;
  pacote[7] = 123; pacote[23] = 1;
  pacote[29] = 222; pacote[29 + 15] = 6;
  exigir(real.processar(pacote, sizeof(pacote), 100) == DashboardF1::Resultado::Atualizado,
         "recebe telemetria real antes da demonstracao");
  demo.aguardar(100);
  demo.atualizar(10100);
  demo.atualizar(43100);
  exigir(real.carro.velocidadeKmh == 222 && real.carro.sessao == 123,
         "demonstracao nao modifica dados nem identificacao da sessao real");
  pacote[23] = 2; pacote[29] = 201;
  exigir(real.processar(pacote, sizeof(pacote), 43200) == DashboardF1::Resultado::Atualizado,
         "receptor real continua aceitando quadros durante a demonstracao");
  exigir(demo.registrarTelemetriaReal(43200) && !demo.ativa(),
         "primeiro pacote real encerra imediatamente a demonstracao");
  exigir(!demo.dados().carroRecente(43200) && real.carro.velocidadeKmh == 201,
         "saida limpa dados falsos e preserva os dados reais recebidos");
  for (uint32_t t = 44000; t <= 60000; t += 1000) {
    demo.registrarTelemetriaReal(t);
    demo.atualizar(t);
    exigir(!demo.ativa(), "trafego real continuo impede a demonstracao");
  }
  exigir(!demo.atualizar(69999) && !demo.ativa(), "queda de jogo respeita nova espera de dez segundos");
  exigir(demo.atualizar(70000) && demo.ativa() && demo.dados().carro.velocidadeKmh == 60,
         "sem jogo novamente, reinicia a demonstracao do inicio");
}

void testarViradaMillis() {
  DemonstracaoDashboard::Demonstracao demo;
  constexpr uint32_t inicio = 0xFFFFF000UL;
  demo.aguardar(inicio);
  exigir(!demo.atualizar(inicio + 9999U) && !demo.ativa(), "espera tolera a virada de millis");
  exigir(demo.atualizar(inicio + 10000U) && demo.ativa(), "inicio tolera a virada de millis");
  demo.registrarTelemetriaReal(inicio + 10050U);
  exigir(!demo.ativa(), "prioridade real tolera a virada de millis");
}
}  // namespace

int main() {
  testarCiclo(); testarPrioridadeReal(); testarViradaMillis();
  std::printf("TESTES DEMONSTRACAO: %u aprovadas; %u falhas.\n", aprovadas, falhas);
  return falhas ? 1 : 0;
}
