#include <array>
#include "telemetria_f1.h"

using PacoteTeste = std::array<uint8_t, 1352>;

constexpr PacoteTeste criarPacote(uint8_t jogador, uint8_t marcha = 8) {
  PacoteTeste pacote{};
  pacote[0] = 0xE9;
  pacote[1] = 0x07;
  pacote[2] = 25;
  pacote[5] = 1;
  pacote[6] = 6;
  for (uint8_t indice = 0; indice < 8; ++indice) {
    pacote[7 + indice] = indice + 1;
  }
  pacote[23] = 0x78;
  pacote[24] = 0x56;
  pacote[25] = 0x34;
  pacote[26] = 0x12;
  pacote[27] = jogador;
  pacote[28] = 255;
  for (uint8_t carro = 0; carro < 22; ++carro) {
    const size_t inicio = 29 + carro * 60;
    const uint16_t velocidade = 100 + carro;
    const uint16_t rpm = 8000 + carro * 100;
    pacote[inicio] = velocidade & 0xFF;
    pacote[inicio + 1] = velocidade >> 8;
    pacote[inicio + 15] = marcha;
    pacote[inicio + 16] = rpm & 0xFF;
    pacote[inicio + 17] = rpm >> 8;
    pacote[inicio + 19] = 75;
  }
  return pacote;
}

constexpr PacoteTeste alterar(PacoteTeste pacote, size_t indice, uint8_t valor) {
  pacote[indice] = valor;
  return pacote;
}

constexpr auto primeiro = criarPacote(0);
constexpr auto ultimo = criarPacote(21);
constexpr auto re = criarPacote(0, 255);
constexpr auto neutro = criarPacote(0, 0);
constexpr auto indiceInvalido = criarPacote(22);
constexpr auto marchaInvalida = criarPacote(0, 9);
constexpr auto formatoErrado = alterar(primeiro, 0, 0xEA);
constexpr auto versaoErrada = alterar(primeiro, 5, 2);
constexpr auto outroTipo = alterar(primeiro, 6, 0);
constexpr auto tipoInvalido = alterar(primeiro, 6, 16);
constexpr auto luzesInvalidas = alterar(primeiro, 29 + 19, 101);
constexpr auto interpretado = F1::interpretar(ultimo.data(), ultimo.size());

static_assert(F1::interpretar(primeiro.data(), primeiro.size()).resultado ==
              F1::Resultado::Telemetria);
static_assert(interpretado.resultado == F1::Resultado::Telemetria);
static_assert(interpretado.dados.indiceCarro == 21);
static_assert(interpretado.dados.velocidadeKmh == 121);
static_assert(interpretado.dados.rpm == 10100);
static_assert(interpretado.dados.marcha == 8);
static_assert(interpretado.dados.luzesRpm == 75);
static_assert(interpretado.dados.sessao == 0x0807060504030201ULL);
static_assert(interpretado.dados.quadro == 0x12345678UL);
static_assert(F1::interpretar(re.data(), re.size()).dados.marcha == -1);
static_assert(F1::interpretar(neutro.data(), neutro.size()).dados.marcha == 0);
static_assert(F1::interpretar(nullptr, 1352).resultado ==
              F1::Resultado::TamanhoInvalido);
static_assert(F1::interpretar(primeiro.data(), 28).resultado ==
              F1::Resultado::TamanhoInvalido);
static_assert(F1::interpretar(primeiro.data(), 1351).resultado ==
              F1::Resultado::TamanhoInvalido);
static_assert(F1::interpretar(primeiro.data(), 1353).resultado ==
              F1::Resultado::TamanhoInvalido);
static_assert(F1::interpretar(indiceInvalido.data(), indiceInvalido.size())
                  .resultado == F1::Resultado::IndiceInvalido);
static_assert(F1::interpretar(formatoErrado.data(), formatoErrado.size())
                  .resultado == F1::Resultado::FormatoIncompativel);
static_assert(F1::interpretar(versaoErrada.data(), versaoErrada.size()).resultado ==
              F1::Resultado::VersaoIncompativel);
static_assert(F1::interpretar(outroTipo.data(), 29).resultado ==
              F1::Resultado::OutroTipo);
static_assert(F1::interpretar(tipoInvalido.data(), tipoInvalido.size()).resultado ==
              F1::Resultado::ValorInvalido);
static_assert(F1::interpretar(marchaInvalida.data(), marchaInvalida.size())
                  .resultado == F1::Resultado::ValorInvalido);
static_assert(F1::interpretar(luzesInvalidas.data(), luzesInvalidas.size())
                  .resultado == F1::Resultado::ValorInvalido);
static_assert(F1::quadroMaisRecente(2, 1));
static_assert(!F1::quadroMaisRecente(1, 1));
static_assert(!F1::quadroMaisRecente(1, 2));
static_assert(F1::quadroMaisRecente(2, 0xFFFFFFFEUL));
