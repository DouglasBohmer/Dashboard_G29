#include "configuracao_tela.h"
#include "indicadores_painel.h"

constexpr size_t contarRpm(const Indicadores::Estado &estado) {
  size_t quantidade = 0;
  for (size_t indice = 0; indice < Indicadores::QUANTIDADE_RPM; ++indice) {
    quantidade += estado[indice];
  }
  return quantidade;
}

constexpr bool conferirPinagem() {
  constexpr uint8_t esperados[] = {13, 14, 16, 17, 25, 27, 26, 4, 19, 21, 5, 15, 2, 12};
  for (size_t indice = 0; indice < Indicadores::PINOS.size(); ++indice) {
    if (Indicadores::PINOS[indice] != esperados[indice]) return false;
    for (size_t outro = indice + 1; outro < Indicadores::PINOS.size(); ++outro) {
      if (Indicadores::PINOS[indice] == Indicadores::PINOS[outro]) return false;
    }
  }
  return true;
}

constexpr bool conferirProgressao() {
  for (int percentual = 0; percentual <= 100; ++percentual) {
    const auto estado = Indicadores::calcularEstado(percentual, false, false, -1, 0);
    const size_t esperado = percentual == 0 ? 0 : (percentual + 9) / 10;
    if (contarRpm(estado) != esperado) return false;
    for (size_t indice = 1; indice < Indicadores::QUANTIDADE_RPM; ++indice) {
      if (estado[indice] && !estado[indice - 1]) return false;
    }
  }
  return true;
}

constexpr auto apagados = Indicadores::calcularEstado(0, false, false, -1, 0);
constexpr auto disponivel = Indicadores::calcularEstado(0, true, false, 0, 0);
constexpr auto aberto = Indicadores::calcularEstado(0, false, true, 0, 0);
constexpr auto amarela = Indicadores::calcularEstado(0, false, false, 3, 0);
constexpr auto verde = Indicadores::calcularEstado(0, false, false, 1, 0);
constexpr auto azul = Indicadores::calcularEstado(0, false, false, 2, 0);

static_assert(conferirPinagem(), "Pinagem final e canais unicos");
static_assert(Indicadores::PINOS.size() == 14, "Dez RPM, dois DRS e dois canais laterais");
static_assert(conferirProgressao(), "Progressao dos dez LEDs em todo o intervalo");
static_assert(contarRpm(apagados) == 0);
static_assert(contarRpm(Indicadores::calcularEstado(-1, false, false, -1, 0)) == 0);
static_assert(contarRpm(Indicadores::calcularEstado(1, false, false, -1, 0)) == 1);
static_assert(contarRpm(Indicadores::calcularEstado(10, false, false, -1, 0)) == 1);
static_assert(contarRpm(Indicadores::calcularEstado(11, false, false, -1, 0)) == 2);
static_assert(contarRpm(Indicadores::calcularEstado(90, false, false, -1, 0)) == 9);
static_assert(contarRpm(Indicadores::calcularEstado(100, false, false, -1, 0)) == 10);
static_assert(contarRpm(Indicadores::calcularEstado(200, false, false, -1, 0)) == 10);
static_assert(Indicadores::CORES_RPM[0] == 0x07E0 && Indicadores::CORES_RPM[1] == 0x07E0);
static_assert(Indicadores::CORES_RPM[2] == 0xFFE0 && Indicadores::CORES_RPM[3] == 0xFFE0);
static_assert(Indicadores::CORES_RPM[4] == 0xF800 && Indicadores::CORES_RPM[6] == 0xF800);
static_assert(Indicadores::CORES_RPM[7] == 0x041F && Indicadores::CORES_RPM[9] == 0x041F);
static_assert(disponivel[Indicadores::DRS_DISPONIVEL] && !disponivel[Indicadores::DRS_ABERTO]);
static_assert(aberto[Indicadores::DRS_ABERTO] && aberto[Indicadores::DRS_DISPONIVEL]);
static_assert(amarela[Indicadores::BANDEIRA_AMARELA] && !amarela[Indicadores::BANDEIRA_VERDE]);
static_assert(verde[Indicadores::BANDEIRA_VERDE] && !verde[Indicadores::BANDEIRA_AMARELA]);
static_assert(!azul[Indicadores::BANDEIRA_AMARELA] && !azul[Indicadores::BANDEIRA_VERDE]);
static_assert(azul[Indicadores::DRS_DISPONIVEL] && azul[Indicadores::DRS_ABERTO]);
static_assert(!Indicadores::calcularEstado(0, true, true, 2, 500)[Indicadores::DRS_DISPONIVEL]);
static_assert(!Indicadores::calcularEstado(0, true, true, 2, 999)[Indicadores::DRS_ABERTO]);
static_assert(Indicadores::calcularEstado(0, false, false, 2, 1000)[Indicadores::DRS_ABERTO]);
static_assert(Indicadores::calcularEstado(0, true, false, 0, 1500)[Indicadores::DRS_DISPONIVEL]);
static_assert(!Indicadores::calcularEstado(0, true, false, 0, 1500)[Indicadores::DRS_ABERTO]);
static_assert(!apagados[Indicadores::BANDEIRA_AMARELA] && !apagados[Indicadores::BANDEIRA_VERDE]);
static_assert(Indicadores::calcularEstado(0, false, false, 3, 499)[Indicadores::BANDEIRA_AMARELA]);
static_assert(!Indicadores::calcularEstado(0, false, false, 3, 500)[Indicadores::BANDEIRA_AMARELA]);
static_assert(!Indicadores::calcularEstado(0, false, false, 1, 999)[Indicadores::BANDEIRA_VERDE]);
static_assert(Indicadores::calcularEstado(0, false, false, 1, 1000)[Indicadores::BANDEIRA_VERDE]);
static_assert(Indicadores::calcularEstado(0, false, true, 3, 500)[Indicadores::DRS_DISPONIVEL]);
static_assert(contarRpm(Indicadores::calcularEstado(10, false, true, 3, 500)) == 1);
static_assert(Configuracao::PINO_CS == -1 && Configuracao::PINO_RESET == -1);
static_assert(Configuracao::PINO_MISO == 35);
