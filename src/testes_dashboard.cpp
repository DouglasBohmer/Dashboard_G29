#include <Arduino.h>
#include <cstring>
#include "dashboard_f1.h"
#include "tela_dashboard.h"

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

void executarTestes() {
  aprovadas = falhas = 0;
  testarValidacao(); testarCarroESequencia(); testarEnergia(); testarIntervalos(); testarHistoricos();
  exigir(InterfaceDashboard::formatarTempo(83418) == "1:23.418", "formatacao do tempo");
  exigir(InterfaceDashboard::formatarTempo(0) == "--:--.---", "tempo ausente");
  exigir(InterfaceDashboard::formatarIntervalo({true, 842, 0}) == "+0.842 s", "formatacao do intervalo");
  exigir(InterfaceDashboard::formatarIntervalo({true, 0, 2}) == "+2V", "formatacao de retardatario");
  exigir(std::strcmp(InterfaceDashboard::nomeModo(3), "ULTRAPASSAGEM") == 0, "nome do modo ERS");
  Serial.printf("TESTES DASHBOARD: %u aprovadas; %u falhas.\n", aprovadas, falhas);
}
}

void setup() { Serial.begin(115200); delay(1000); executarTestes(); }
void loop() {
  if (Serial.available() && Serial.read() == 't') executarTestes();
  delay(10);
}
