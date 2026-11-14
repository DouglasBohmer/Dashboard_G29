#include <Arduino.h>
#include <cstring>
#include "dashboard_f1.h"
#include "tela_dashboard.h"
#include "indicadores_painel.h"
#include "perfis_dashboard.h"
#include "cores_dashboard.h"

namespace {
uint8_t pacote[1461]{};
unsigned aprovadas = 0, falhas = 0;
size_t tamanho = 0;
using Resultado = DashboardF1::Resultado;

void exigir(bool condicao, const char *descricao) {
  if (condicao) ++aprovadas;
  else { ++falhas; Serial.printf("FALHA: %s\n", descricao); }
}
void gravar16(size_t posicao, uint16_t valor) {
  pacote[posicao] = valor & 255;
  pacote[posicao + 1] = valor >> 8;
}
void gravar32(size_t posicao, uint32_t valor) {
  for (uint8_t indice = 0; indice < 4; ++indice) pacote[posicao + indice] = valor >> (indice * 8);
}
void gravarFloat(size_t posicao, float valor) {
  uint32_t bits;
  std::memcpy(&bits, &valor, sizeof(bits));
  gravar32(posicao, bits);
}
void preparar(uint8_t tipo, uint32_t quadro = 1, uint8_t sessao = 123) {
  std::memset(pacote, 0, sizeof(pacote));
  gravar16(0, 2025);
  pacote[2] = 25; pacote[5] = 1; pacote[6] = tipo;
  pacote[7] = sessao; gravar32(23, quadro); pacote[28] = 255;
  switch (tipo) {
    case 1: tamanho = 753; break;
    case 2: tamanho = 1285; break;
    case 3: tamanho = 45; break;
    case 4: tamanho = 1284; break;
    case 6: tamanho = 1352; break;
    case 7: tamanho = 1239; break;
    case 11: tamanho = 1460; break;
    default: tamanho = 29; break;
  }
}
void prepararCarro(uint32_t quadro = 1, uint8_t sessao = 123) {
  preparar(6, quadro, sessao);
  gravar16(29, 286); pacote[44] = 7; gravar16(45, 11980);
  pacote[47] = 1; pacote[48] = 92;
}
void prepararHistorico(uint8_t carro, uint32_t quadro = 1) {
  preparar(11, quadro);
  pacote[29] = carro; pacote[30] = 3; pacote[32] = 3;
  gravar32(36, 90000); pacote[49] = 1;
  gravar32(50, 70000); pacote[63] = 0;
  gravar32(64, 82906); pacote[77] = 1;
}
void preencherVolta(uint8_t carro, uint8_t posicao, uint16_t intervalo, float distancia) {
  const size_t inicio = 29 + carro * 57;
  gravar32(inicio, 84250);
  gravar32(inicio + 4, 83418);
  gravar16(inicio + 14, intervalo);
  gravarFloat(inicio + 24, distancia);
  pacote[inicio + 32] = posicao; pacote[inicio + 33] = 12; pacote[inicio + 45] = 2;
}

void testarValidacao() {
  DashboardF1::Telemetria dados;
  exigir(dados.processar(nullptr, 1460, 0) == Resultado::TamanhoInvalido, "ponteiro nulo");
  prepararCarro();
  exigir(dados.processar(pacote, 28, 0) == Resultado::TamanhoInvalido, "cabecalho curto");
  exigir(dados.processar(pacote, 1351, 0) == Resultado::TamanhoInvalido, "telemetria truncada");
  exigir(dados.processar(pacote, 1353, 0) == Resultado::TamanhoInvalido, "telemetria excedente");
  pacote[0] = 0xEA;
  exigir(dados.processar(pacote, tamanho, 0) == Resultado::FormatoIncompativel, "formato 2026 recusado");
  prepararCarro(); pacote[5] = 2;
  exigir(dados.processar(pacote, tamanho, 0) == Resultado::VersaoIncompativel, "versao recusada");
  prepararCarro(); pacote[27] = 22;
  exigir(dados.processar(pacote, tamanho, 0) == Resultado::IndiceInvalido, "indice fora do array");
  preparar(16);
  exigir(dados.processar(pacote, tamanho, 0) == Resultado::ValorInvalido, "tipo inexistente");
  preparar(0);
  exigir(dados.processar(pacote, tamanho, 0) == Resultado::Ignorado, "tipo nao utilizado");
  prepararCarro(); pacote[47] = 2;
  exigir(dados.processar(pacote, tamanho, 0) == Resultado::ValorInvalido, "DRS invalido");
}

void testarCarroESequencia() {
  DashboardF1::Telemetria dados;
  prepararCarro();
  exigir(dados.processar(pacote, tamanho, 100) == Resultado::Atualizado, "carro aceito");
  exigir(dados.carro.velocidadeKmh == 286 && dados.carro.rpm == 11980, "velocidade e RPM nos offsets oficiais");
  exigir(dados.carro.marcha == 7 && dados.carro.drsAtivo && dados.carro.luzesRpm == 92, "marcha DRS e indicacao de troca");
  exigir(dados.processar(pacote, tamanho, 101) == Resultado::ForaDeOrdem, "quadro duplicado");
  gravar32(23, 0);
  exigir(dados.processar(pacote, tamanho, 102) == Resultado::ForaDeOrdem, "quadro antigo");
  gravar32(23, 2); pacote[44] = 255;
  exigir(dados.processar(pacote, tamanho, 103) == Resultado::Atualizado && dados.carro.marcha == -1, "marcha re");
  gravar32(23, 3); pacote[44] = 0;
  exigir(dados.processar(pacote, tamanho, 104) == Resultado::Atualizado && dados.carro.marcha == 0, "neutro");
  exigir(dados.carroRecente(3103) && !dados.carroRecente(3104), "validade de tres segundos");
  dados.reiniciar(); prepararCarro(0xFFFFFFFF);
  dados.processar(pacote, tamanho, 0xFFFFFF00);
  prepararCarro(0);
  exigir(dados.processar(pacote, tamanho, 100) == Resultado::Atualizado, "retorno do contador de quadros");
  exigir(dados.carroRecente(200), "retorno de millis");
  prepararCarro(1, 124);
  dados.processar(pacote, tamanho, 201);
  exigir(!dados.energiaRecente(201) && dados.minhaMelhor(201) == 0, "nova sessao limpa caches");
  prepararCarro(5, 123);
  exigir(dados.processar(pacote, tamanho, 202) == Resultado::ForaDeOrdem, "sessao anterior atrasada");
}

void testarEnergia() {
  DashboardF1::Telemetria dados;
  preparar(1); pacote[32] = 27; gravar16(33, 5793); pacote[35] = 15;
  exigir(dados.processar(pacote, tamanho, 100) == Resultado::Atualizado && dados.totalVoltas == 27 && dados.comprimentoPista == 5793, "sessao e tamanho de pista");
  preparar(7); gravarFloat(29 + 37, 2560000); gravarFloat(29 + 50, 1120000);
  pacote[29 + 41] = 3; pacote[29 + 22] = 1;
  exigir(dados.processar(pacote, tamanho, 100) == Resultado::Atualizado, "estado do carro aceito");
  exigir(dados.energia.armazenada == 2560000 && dados.energia.aplicadaNaVolta == 1120000, "floats IEEE754 little endian");
  exigir(dados.energia.modo == 3 && dados.energia.drsPermitido, "modo e DRS permitido");
  exigir(DashboardF1::percentual(dados.energia.armazenada, 4000000) == 64, "carga da bateria 64 por cento");
  exigir(100 - DashboardF1::percentual(dados.energia.aplicadaNaVolta, 4000000) == 72, "saldo de volta separado da bateria");
  exigir(DashboardF1::percentual(8000000, 4000000) == 100 && DashboardF1::percentual(0, 4000000) == 0, "limites da escala");
  exigir(DashboardF1::percentual(-1, 4000000) == -1 && DashboardF1::percentual(1, 0) == -1, "escala invalida");
  gravar32(23, 2); gravar32(29 + 37, 0x7FC00000);
  exigir(dados.processar(pacote, tamanho, 200) == Resultado::ValorInvalido, "NaN recusado");
  gravarFloat(29 + 37, -1);
  exigir(dados.processar(pacote, tamanho, 200) == Resultado::ValorInvalido, "energia negativa recusada");
  gravarFloat(29 + 37, 1); pacote[29 + 41] = 4;
  exigir(dados.processar(pacote, tamanho, 200) == Resultado::ValorInvalido, "modo ERS desconhecido recusado");
  exigir(dados.energia.armazenada == 2560000 && !dados.energiaRecente(3100), "pacote invalido nao renova dados");
}

void testarIntervalos() {
  DashboardF1::Telemetria dados;
  preparar(1); gravar16(33, 5793); dados.processar(pacote, tamanho, 0);
  preparar(2); preencherVolta(0, 4, 842, 12340); preencherVolta(8, 3, 400, 12385); preencherVolta(2, 5, 1276, 12275);
  pacote[29 + 37] = 1;
  exigir(dados.processar(pacote, tamanho, 100) == Resultado::Atualizado, "voltas aceitas");
  exigir(dados.volta.posicao == 4 && dados.volta.numero == 12 && dados.volta.tempo == 83418 && dados.volta.invalida, "dados da volta atual");
  exigir(dados.volta.anterior == 84250, "ultima volta no primeiro campo de LapData");
  exigir(dados.volta.frente.valido && dados.volta.frente.milissegundos == 842, "intervalo da frente");
  exigir(dados.volta.atras.valido && dados.volta.atras.milissegundos == 1276, "perseguidor selecionado por posicao e nao por indice");
  gravar32(23, 2); pacote[29 + 16] = 1;
  dados.processar(pacote, tamanho, 101);
  exigir(dados.volta.frente.milissegundos == 60842, "intervalo com minutos");
  gravar32(23, 3); gravar16(29 + 14, 60000);
  dados.processar(pacote, tamanho, 102);
  exigir(!dados.volta.frente.valido, "parte de intervalo invalida");
  gravar32(23, 4); gravarFloat(29 + 8 * 57 + 24, 25000);
  dados.processar(pacote, tamanho, 103);
  exigir(dados.volta.frente.voltas == 2, "diferenca de voltas nao vira segundos falsos");
  preparar(2, 5); preencherVolta(0, 1, 0, 12000); preencherVolta(2, 2, 1500, 11900);
  dados.processar(pacote, tamanho, 104);
  exigir(!dados.volta.frente.valido && dados.volta.atras.milissegundos == 1500, "lider sem referencia a frente");
}

void testarHistoricos() {
  DashboardF1::Telemetria dados;
  prepararHistorico(0, 10);
  exigir(dados.processar(pacote, tamanho, 100) == Resultado::Atualizado, "historico aceito");
  exigir(dados.minhaMelhor(100) == 82906, "volta invalida mais rapida ignorada");
  prepararHistorico(2, 10); gravar32(64, 81884);
  exigir(dados.processar(pacote, tamanho, 100) == Resultado::Atualizado, "dois carros no mesmo quadro");
  exigir(dados.melhorProva(100) == 81884 && dados.minhaMelhor(100) == 82906, "melhor da prova diferente da pessoal");
  exigir(dados.processar(pacote, tamanho, 101) == Resultado::ForaDeOrdem, "historico duplicado por carro");
  exigir(dados.melhorProva(10100) == 0, "historico expira");
  prepararHistorico(22, 11);
  exigir(dados.processar(pacote, tamanho, 200) == Resultado::IndiceInvalido, "indice do historico invalido");
  prepararHistorico(0, 11); pacote[30] = 101;
  exigir(dados.processar(pacote, tamanho, 200) == Resultado::ValorInvalido, "historico acima de cem voltas");
  preparar(3, 50); std::memcpy(pacote + 29, "FLBK", 4);
  exigir(dados.processar(pacote, tamanho, 201) == Resultado::Atualizado && dados.melhorProva(201) == 0, "flashback limpa melhores");
  prepararHistorico(0, 49);
  exigir(dados.processar(pacote, tamanho, 202) == Resultado::ForaDeOrdem, "historico anterior ao flashback recusado");
  prepararHistorico(0, 51); gravar32(64, 95000);
  exigir(dados.processar(pacote, tamanho, 203) == Resultado::Atualizado && dados.minhaMelhor(203) == 90000, "historico reconstroi melhor apos flashback");
}

void testarBandeirasEPausa() {
  DashboardF1::Telemetria dados;
  preparar(7); pacote[29 + 28] = 3;
  exigir(dados.processar(pacote, tamanho, 100) == Resultado::Atualizado &&
         dados.bandeiraFia == 3, "bandeira amarela no offset oficial");
  auto estado = Indicadores::calcularEstado(40, true, false, dados.bandeiraFia, 0);
  exigir(estado[Indicadores::BANDEIRA_AMARELA] && !estado[Indicadores::BANDEIRA_VERDE],
         "um canal para os dois amarelos");
  exigir(estado[3] && !estado[4] && estado[Indicadores::DRS_DISPONIVEL],
         "RPM progressivo e DRS independente");
  preparar(7, 2); pacote[29 + 28] = 1;
  exigir(dados.processar(pacote, tamanho, 101) == Resultado::Atualizado &&
         dados.bandeiraFia == 1, "bandeira verde");
  estado = Indicadores::calcularEstado(0, false, true, dados.bandeiraFia, 0);
  exigir(estado[Indicadores::BANDEIRA_VERDE] && !estado[Indicadores::BANDEIRA_AMARELA] &&
         estado[Indicadores::DRS_ABERTO] && estado[Indicadores::DRS_DISPONIVEL],
         "verde e as duas luzes DRS com asa aberta");
  estado = Indicadores::calcularEstado(0, false, true, dados.bandeiraFia, 500);
  exigir(!estado[Indicadores::BANDEIRA_VERDE] && estado[Indicadores::DRS_DISPONIVEL] &&
         estado[Indicadores::DRS_ABERTO], "fase apagada da bandeira preserva os dois DRS");
  preparar(7, 3); pacote[29 + 28] = 255;
  exigir(dados.processar(pacote, tamanho, 102) == Resultado::Atualizado &&
         dados.bandeiraFia == -1, "bandeira desconhecida com sinal");
  preparar(7, 4); pacote[29 + 28] = 4;
  exigir(dados.processar(pacote, tamanho, 103) == Resultado::ValorInvalido &&
         dados.bandeiraFia == -1, "bandeira invalida preserva estado anterior");
  preparar(7, 5); pacote[29 + 54] = 1;
  dados.processar(pacote, tamanho, 104);
  exigir(dados.pausado(104) && !dados.pausado(3104), "pausa de rede expira com o pacote");
  preparar(7, 6); pacote[29 + 54] = 2;
  exigir(dados.processar(pacote, tamanho, 105) == Resultado::ValorInvalido,
         "estado de pausa invalido");
  preparar(1); pacote[43] = 1;
  dados.processar(pacote, tamanho, 106);
  exigir(dados.pausado(106), "pausa da sessao");
  preparar(1, 2); pacote[43] = 0;
  dados.processar(pacote, tamanho, 107);
  preparar(7, 7); pacote[29 + 54] = 0;
  dados.processar(pacote, tamanho, 107);
  exigir(!dados.pausado(107), "retomada da sessao e do carro");
  dados.reiniciar();
  exigir(dados.bandeiraFia == -1 && !dados.pausado(108), "reinicio limpa bandeira e pausa");
}

void testarParticipantes() {
  DashboardF1::Telemetria dados;
  preparar(4); pacote[29] = 9;
  pacote[30 + 8 * 57] = 1;
  std::memcpy(pacote + 30 + 8 * 57 + 7, "Alexander Albon", 16);
  std::memcpy(pacote + 30 + 2 * 57 + 7, "PilotoRede", 11);
  exigir(dados.processar(pacote, tamanho - 1, 0) == Resultado::TamanhoInvalido,
         "participantes truncados recusados");
  exigir(dados.processar(pacote, tamanho, 0) == Resultado::Atualizado &&
         std::strcmp(dados.nomePiloto(8), "Alexander Albon") == 0, "nome no offset de participantes 2025");
  preparar(2); preencherVolta(0, 8, 842, 12340); preencherVolta(8, 7, 400, 12385);
  preencherVolta(2, 9, 1276, 12275);
  dados.processar(pacote, tamanho, 1);
  exigir(dados.volta.indiceFrente == 8 && dados.volta.indiceAtras == 2,
         "indices dos vizinhos definidos pela classificacao");
  exigir(InterfaceDashboard::formatarVizinho(dados, true) == "7\xA7 - Albon",
         "vizinho com posicao ordinal e sobrenome IA");
  exigir(InterfaceDashboard::formatarVizinho(dados, false) == "9\xA7 - PilotoRede",
         "vizinho humano preserva o apelido");
  preparar(2, 2); preencherVolta(0, 8, 842, 12340); preencherVolta(2, 7, 400, 12385);
  preencherVolta(8, 9, 1276, 12275); dados.processar(pacote, tamanho, 2);
  exigir(InterfaceDashboard::formatarVizinho(dados, true) == "7\xA7 - PilotoRede" &&
         InterfaceDashboard::formatarVizinho(dados, false) == "9\xA7 - Albon",
         "ultrapassagem troca os nomes junto com a posicao");
  preparar(4, 2); pacote[29] = 23;
  exigir(dados.processar(pacote, tamanho, 2) == Resultado::ValorInvalido &&
         std::strcmp(dados.nomePiloto(8), "Alexander Albon") == 0, "pacote invalido preserva nomes");
  pacote[29] = 9; pacote[30 + 8 * 57 + 44] = 5;
  exigir(dados.processar(pacote, tamanho, 2) == Resultado::ValorInvalido,
         "quantidade de cores invalida");
  pacote[30 + 8 * 57 + 44] = 0;
  std::memset(pacote + 30 + 8 * 57 + 7, 'X', 32);
  dados.processar(pacote, tamanho, 2);
  exigir(std::strlen(dados.nomePiloto(8)) == 32 &&
         InterfaceDashboard::formatarVizinho(dados, false).length() == 16,
         "nome sem terminador fica limitado e cabe na tela");
  exigir(InterfaceDashboard::nomeExibivel("Nico H\xC3\xBClkenberg") == "Nico Hulkenberg",
         "nome UTF-8 convertido para a fonte do display");
  preparar(2, 3); preencherVolta(0, 1, 0, 12000);
  dados.processar(pacote, tamanho, 3);
  exigir(InterfaceDashboard::formatarVizinho(dados, true) == "--", "lider sem vizinho a frente");
  prepararCarro(1, 124); dados.processar(pacote, tamanho, 4);
  exigir(dados.nomePiloto(8)[0] == '\0', "mudanca de sessao limpa participantes");
}

void testarDeltaUltimaVolta() {
  DashboardF1::Telemetria dados;
  preparar(1); gravar16(33, 5000); dados.processar(pacote, tamanho, 0);
  uint32_t quadro = 1;
  auto amostra = [&](uint8_t volta, int ponto, uint32_t tempo, uint32_t anterior, bool valida = true) {
    preparar(2, quadro++); preencherVolta(0, 1, 0, 0);
    gravarFloat(29 + 20, ponto * 5000.0f / 120);
    gravar32(29 + 4, tempo); gravar32(29, anterior);
    pacote[29 + 33] = volta; pacote[29 + 37] = !valida; pacote[29 + 44] = 4;
    dados.processar(pacote, tamanho, quadro);
  };
  for (int ponto = 0; ponto < 120; ++ponto) amostra(1, ponto, ponto * 500, 0);
  exigir(!dados.volta.delta.valido, "primeira volta grava referencia sem delta inventado");
  amostra(2, 0, 0, 60000);
  exigir(dados.volta.delta.valido && dados.volta.delta.milissegundos == 0,
         "volta completa habilita comparacao no mesmo ponto");
  for (int ponto = 1; ponto <= 60; ++ponto) amostra(2, ponto, ponto * 495, 60000);
  exigir(dados.volta.delta.valido && std::abs(dados.volta.delta.milissegundos + 300) <= 1,
         "delta negativo indica trezentos ms de ganho na metade da pista");
  for (int ponto = 61; ponto < 120; ++ponto) amostra(2, ponto, ponto * 495, 60000);
  amostra(3, 0, 0, 59400);
  for (int ponto = 1; ponto <= 60; ++ponto) amostra(3, ponto, ponto * 505, 59400);
  exigir(dados.volta.delta.valido && std::abs(dados.volta.delta.milissegundos - 600) <= 1,
         "referencia passa a ser a ultima volta e delta positivo indica perda");
  amostra(3, 61, 30805, 59400, false);
  exigir(!dados.volta.delta.valido, "volta invalida oculta delta");
  amostra(4, 0, 0, 60600);
  exigir(!dados.volta.delta.valido, "volta invalida nao se torna referencia");
  amostra(4, 30, 15000, 60600);
  amostra(5, 0, 0, 60000);
  exigir(!dados.volta.delta.valido, "lacuna grande nao cria uma volta de referencia parcial");
  for (int ponto = 1; ponto < 120; ++ponto) amostra(5, ponto, ponto * 500, 60000);
  amostra(6, 0, 0, 60000);
  exigir(dados.volta.delta.valido, "referencia volta apos uma nova volta completa");
  preparar(3, quadro++); std::memcpy(pacote + 29, "FLBK", 4);
  dados.processar(pacote, tamanho, quadro);
  amostra(5, 60, 30000, 60000);
  exigir(!dados.volta.delta.valido, "flashback limpa curva de referencia");
  exigir(InterfaceDashboard::formatarDelta({true, -842}) == "-0.842" &&
         InterfaceDashboard::formatarDelta({true, 1276}) == "+1.276" &&
         InterfaceDashboard::formatarDelta({}) == "--", "delta formatado com sinal e milissegundos");
}

void testarDestinoF1() {
  using RedePainel::IPv4;
  constexpr IPv4 mascara{255, 255, 255, 0};
  constexpr IPv4 rede8{192, 168, 8, 30}, rede1{192, 168, 1, 14};
  exigir(RedePainel::candidato(rede8, mascara, 0) == IPv4{192, 168, 8, 80},
         "IP preferido na rede 192.168.8.x");
  bool alternativasCorretas = true;
  for (uint8_t opcao = 1; opcao < 5; ++opcao)
    alternativasCorretas &= RedePainel::candidato(rede8, mascara, opcao) ==
                           IPv4{192, 168, 8, uint8_t(80 + opcao)};
  exigir(alternativasCorretas, "quatro alternativas de IP distintas .81 a .84");
  exigir(RedePainel::candidato(rede1, mascara, 0) == IPv4{192, 168, 1, 80},
         "outra sub-rede preserva alcance local e terminacao .80");
  exigir(RedePainel::candidato(rede1, mascara, 4) == IPv4{192, 168, 1, 84},
         "ultima alternativa acompanha a sub-rede real");
  exigir(RedePainel::candidato(rede1, {255, 255, 0, 0}, 0) == RedePainel::PREFERIDO,
         "mascara ampla aceita o IP preferido quando alcancavel");
  exigir(RedePainel::candidato(rede8, {255, 255, 255, 240}, 0) == IPv4{},
         "sub-rede pequena nao recebe host fora da sua faixa");
  exigir(RedePainel::candidato(rede8, {}, 0) == IPv4{} &&
         RedePainel::candidato(rede8, mascara, 5) == IPv4{},
         "mascara ausente e opcao inexistente nao geram IP");
  exigir(!RedePainel::utilizavel({192, 168, 8, 0}, rede8, mascara) &&
         !RedePainel::utilizavel({192, 168, 8, 255}, rede8, mascara) &&
         !RedePainel::utilizavel({192, 168, 1, 80}, rede8, mascara),
         "enderecos de rede, broadcast e outra sub-rede sao rejeitados");
  exigir(InterfaceDashboard::formatarDestinoF1(RedePainel::PREFERIDO, 1) ==
         "1/5 F1>192.168.8.80:20777", "IP e porta sempre identificam o destino no F1");
  exigir(InterfaceDashboard::formatarDestinoF1({192, 168, 8, 84}, 5) ==
         "5/5 F1>192.168.8.84:20777", "tela identifica a alternativa escolhida");
  exigir(InterfaceDashboard::formatarDestinoF1({192, 168, 1, 14}, 0) ==
         "F1>192.168.1.14:20777", "fallback DHCP tambem mostra destino real");
  exigir(InterfaceDashboard::formatarDestinoF1({255, 255, 255, 254}, 5).length() * 6 <= 154,
         "IP longo cabe no campo sem perder a porta");
}

void testarTemperaturas() {
  DashboardF1::Telemetria dados;
  prepararCarro();
  for (uint8_t roda = 0; roda < 4; ++roda) {
    gravar16(29 + 22 + roda * 2, 300 + roda * 230);
    pacote[29 + 30 + roda] = 96 + roda;
    pacote[29 + 34 + roda] = 85 + roda;
  }
  exigir(dados.processar(pacote, tamanho, 100) == Resultado::Atualizado,
         "telemetria com temperaturas aceita");
  for (uint8_t roda = 0; roda < 4; ++roda)
    exigir(dados.carro.temperaturaFreios[roda] == 300 + roda * 230 &&
           dados.carro.temperaturaPneusInterna[roda] == 85 + roda &&
           dados.carro.temperaturaPneusExterna[roda] == 96 + roda,
           "temperaturas em Celsius nos offsets e ordem oficiais, sem trocar interno e externo");
  exigir(F1::DianteiraEsquerda == 2 && F1::DianteiraDireita == 3 &&
         F1::TraseiraEsquerda == 0 && F1::TraseiraDireita == 1,
         "mapeamento fisico das rodas da tela");
  exigir(dados.carroRecente(3099) && !dados.carroRecente(3100),
         "temperaturas vencem junto com a telemetria do carro");
  prepararCarro(2); pacote[27] = 21;
  const size_t base = 29 + 21 * F1::TAMANHO_CARRO;
  gravar16(base + 22 + 2 * 2, 1123);
  pacote[base + 30 + 2] = 119; pacote[base + 34 + 2] = 103;
  exigir(dados.processar(pacote, tamanho, 200) == Resultado::Atualizado &&
         dados.carro.temperaturaFreios[F1::DianteiraEsquerda] == 1123 &&
         dados.carro.temperaturaPneusExterna[F1::DianteiraEsquerda] == 119 &&
         dados.carro.temperaturaPneusInterna[F1::DianteiraEsquerda] == 103,
         "temperaturas usam o indice real do jogador, inclusive o ultimo carro");
  exigir(dados.processar(pacote, tamanho - 1, 300) == Resultado::TamanhoInvalido &&
         dados.carro.temperaturaFreios[2] == 1123,
         "pacote truncado nao altera as temperaturas");
  using namespace InterfaceDashboard;
  exigir(corTemperaturaPneu(69) == 0x041F && corTemperaturaPneu(70) == 0x07FF &&
         corTemperaturaPneu(84) == 0x07FF && corTemperaturaPneu(85) == 0x07E0 &&
         corTemperaturaPneu(104) == 0x07E0 && corTemperaturaPneu(105) == 0xFFE0 &&
         corTemperaturaPneu(114) == 0xFFE0 && corTemperaturaPneu(115) == 0xF800,
         "faixas de cor dos pneus respeitam todos os limites");
  exigir(corTemperaturaFreio(299) == 0x041F && corTemperaturaFreio(300) == 0x07FF &&
         corTemperaturaFreio(499) == 0x07FF && corTemperaturaFreio(500) == 0x07E0 &&
         corTemperaturaFreio(849) == 0x07E0 && corTemperaturaFreio(850) == 0xFFE0 &&
         corTemperaturaFreio(999) == 0xFFE0 && corTemperaturaFreio(1000) == 0xF800 &&
         corTemperaturaFreio(65535) == 0xF800,
         "fundo dos freios acompanha temperatura sem estouro no limite uint16");
  exigir(contrasteTemperatura(0x041F) == 0xFFFF && contrasteTemperatura(0xF800) == 0xFFFF &&
         contrasteTemperatura(0x07E0) == 0 && contrasteTemperatura(0xFFE0) == 0,
         "texto contrasta com os fundos coloridos");
  exigir(estiloErs(true, 0).fundo != estiloErs(true, 1).fundo &&
         estiloErs(true, 1).fundo != estiloErs(true, 2).fundo &&
         estiloErs(true, 2).fundo != estiloErs(true, 3).fundo &&
         estiloErs(false, 3).fundo == estiloErs(false, 0).fundo,
         "ERS usa cores por modo e estado ausente neutro");
}

void testarAcaoUdpETelas() {
  DashboardF1::Telemetria dados;
  PerfisDashboard::Selecao telas;
  auto botao = [&](uint32_t quadro, uint32_t estado, uint8_t sessao = 123) {
    preparar(3, quadro, sessao); std::memcpy(pacote + 29, "BUTN", 4);
    gravar32(33, estado);
    return dados.processar(pacote, tamanho, quadro);
  };
  exigir(!dados.consumirAcaoUdp1() && telas.atual().pagina == PerfisDashboard::Pagina::Principal,
         "inicia na principal sem acao pendente");
  prepararCarro(); dados.processar(pacote, tamanho, 100);
  exigir(botao(1, 0x00100000UL) == Resultado::Atualizado && dados.consumirAcaoUdp1(),
         "BUTN da Acao UDP 1 usa o bit oficial 20");
  telas.proxima();
  exigir(telas.atual().pagina == PerfisDashboard::Pagina::Temperaturas &&
         !dados.consumirAcaoUdp1() && dados.carroRecente(100),
         "acao consumida uma vez, tela de pneus, telemetria preservada");
  exigir(botao(1, 0x00100000UL) == Resultado::ForaDeOrdem && !dados.consumirAcaoUdp1(),
         "duplicata UDP nao repete a troca");
  exigir(botao(2, 0x00100000UL | 4) == Resultado::Atualizado && !dados.consumirAcaoUdp1(),
         "botao segurado e outro botao pressionado nao repetem a troca");
  exigir(botao(2, 0) == Resultado::Atualizado && !dados.consumirAcaoUdp1(),
         "soltar no mesmo quadro rearma sem trocar a pagina");
  exigir(botao(1, 0x00100000UL) == Resultado::ForaDeOrdem && !dados.consumirAcaoUdp1(),
         "evento antigo nao volta a pressionar o botao");
  exigir(botao(3, 0x00100000UL) == Resultado::Atualizado && dados.consumirAcaoUdp1(),
         "segundo toque gera nova acao");
  telas.proxima();
  exigir(telas.atual().pagina == PerfisDashboard::Pagina::Principal,
         "segunda acao volta para a principal");
  botao(4, 0); botao(5, 0x00200000UL);
  exigir(!dados.consumirAcaoUdp1(), "Acao UDP 2 e ignorada pelo seletor");
  preparar(3, 5); std::memcpy(pacote + 29, "FLBK", 4);
  exigir(dados.processar(pacote, tamanho, 5) == Resultado::Atualizado,
         "flashback e botoes podem ocorrer no mesmo quadro sem se bloquear");
  botao(6, 0x00100000UL); dados.reiniciar();
  exigir(!dados.consumirAcaoUdp1(), "reinicio limpa a acao pendente");
  exigir(botao(1, 0x00100000UL, 124) == Resultado::Atualizado && dados.consumirAcaoUdp1(),
         "nova sessao aceita o primeiro toque sem herdar botoes antigos");
  botao(2, 0, 124); pacote[5] = 2; gravar32(33, 0x00100000UL); gravar32(23, 3);
  exigir(dados.processar(pacote, tamanho, 3) == Resultado::VersaoIncompativel &&
         !dados.consumirAcaoUdp1(), "evento de versao invalida nao alterna tela");
  preparar(3, 3, 124); std::memcpy(pacote + 29, "BUTN", 4); gravar32(33, 0x00100000UL);
  exigir(dados.processar(pacote, 36, 3) == Resultado::TamanhoInvalido &&
         !dados.consumirAcaoUdp1(), "evento truncado nao le botoes fora do pacote");
}

void executarTestes() {
  aprovadas = falhas = 0;
  testarValidacao(); testarCarroESequencia(); testarEnergia(); testarIntervalos(); testarHistoricos();
  testarBandeirasEPausa();
  testarParticipantes(); testarDeltaUltimaVolta();
  testarDestinoF1();
  testarTemperaturas(); testarAcaoUdpETelas();
  exigir(PerfisDashboard::DISPONIVEIS.size() == 2 &&
         PerfisDashboard::compativel(PerfisDashboard::ATIVO, 2025, 0) &&
         !PerfisDashboard::compativel(PerfisDashboard::ATIVO, 2025, 2) &&
         !PerfisDashboard::compativel(PerfisDashboard::ATIVO, 2026, 0) &&
         PerfisDashboard::compativel(PerfisDashboard::TEMPERATURAS_F1_2025, 2025, 0) &&
         !PerfisDashboard::compativel(PerfisDashboard::TEMPERATURAS_F1_2025, 2026, 0),
         "duas paginas disponiveis apenas para F1 2025 e formula correta");
  exigir(InterfaceDashboard::formatarTempo(83418) == "1:23.418", "formatacao do tempo");
  exigir(InterfaceDashboard::formatarTempo(0) == "--:--.---", "tempo ausente");
  exigir(InterfaceDashboard::formatarIntervalo({true, 842, 0}) == "+0.842 s", "formatacao do intervalo");
  exigir(InterfaceDashboard::formatarIntervalo({true, 0, 2}) == "+2V", "formatacao de retardatario");
  exigir(InterfaceDashboard::formatarIntervalo({true, 842, 0}, '-') == "-0.842 s",
         "D+1 mostra o carro da frente com sinal negativo");
  exigir(InterfaceDashboard::formatarIntervalo({true, 0, 2}, '-') == "-2V",
         "D+1 preserva diferencas de voltas");
  exigir(std::strcmp(InterfaceDashboard::nomeModo(3), "ULTRAPASSAGEM") == 0, "nome do modo ERS");
  exigir(InterfaceDashboard::formatarSensores(204, true, 25.3f, 60.0f) ==
         "Luz:80% T:25.3C U:60.0%", "brilho temperatura e umidade no rodape");
  exigir(InterfaceDashboard::formatarSensores(255, false, NAN, NAN) ==
         "Luz:100% T:-- U:--", "sensor invalido exibe campos pendentes");
  exigir(InterfaceDashboard::formatarSensores(255, true, NAN, NAN) ==
         "Luz:100% T:-- U:--", "valores nao finitos ficam fora do rodape");
  exigir((21 + 3 + InterfaceDashboard::formatarSensores(255, true, -40, 100).length()) * 6 <= 320,
         "rodape cabe na tela com valores extremos e situacao de conexao");
  Serial.printf("TESTES DASHBOARD: %u aprovadas; %u falhas.\n", aprovadas, falhas);
}
}

void setup() {
  for (uint8_t pino : Indicadores::PINOS) {
    digitalWrite(pino, LOW);
    pinMode(pino, OUTPUT);
  }
  Serial.begin(115200);
  delay(1000);
  executarTestes();
}
void loop() {
  if (Serial.available() && Serial.read() == 't') executarTestes();
  delay(10);
}
