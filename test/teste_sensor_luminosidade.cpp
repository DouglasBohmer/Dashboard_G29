#include "sensor_luminosidade.h"

constexpr Luminosidade::Sensor lerEscuro(uint32_t instante) {
  Luminosidade::Sensor sensor;
  sensor.atualizar(ConfiguracaoLuminosidade::NIVEL_ALTO_INDICA_ESCURO, 1000);
  sensor.atualizar(ConfiguracaoLuminosidade::NIVEL_ALTO_INDICA_ESCURO, instante);
  return sensor;
}

constexpr Luminosidade::Sensor mudarParaClaro(uint32_t instante) {
  auto sensor = lerEscuro(1300);
  sensor.atualizar(!ConfiguracaoLuminosidade::NIVEL_ALTO_INDICA_ESCURO, 1500);
  sensor.atualizar(!ConfiguracaoLuminosidade::NIVEL_ALTO_INDICA_ESCURO, instante);
  return sensor;
}

constexpr Luminosidade::Sensor simularOscilacao(uint32_t instante) {
  auto sensor = lerEscuro(1300);
  sensor.atualizar(!ConfiguracaoLuminosidade::NIVEL_ALTO_INDICA_ESCURO, 1400);
  sensor.atualizar(ConfiguracaoLuminosidade::NIVEL_ALTO_INDICA_ESCURO, 1500);
  sensor.atualizar(!ConfiguracaoLuminosidade::NIVEL_ALTO_INDICA_ESCURO, 1600);
  sensor.atualizar(!ConfiguracaoLuminosidade::NIVEL_ALTO_INDICA_ESCURO, instante);
  return sensor;
}

constexpr Luminosidade::Sensor simularViradaRelogio(uint32_t instante) {
  Luminosidade::Sensor sensor;
  sensor.atualizar(ConfiguracaoLuminosidade::NIVEL_ALTO_INDICA_ESCURO, 0xFFFFFF80U);
  sensor.atualizar(ConfiguracaoLuminosidade::NIVEL_ALTO_INDICA_ESCURO, instante);
  return sensor;
}

static_assert(!Luminosidade::Sensor{}.valido(), "Inicio sem leitura confirmada");
static_assert(!Luminosidade::Sensor{}.escuro(), "Inicio preserva tela clara");
static_assert(Luminosidade::Sensor{}.brilhoAlvo() == 255, "Brilho inicial maximo");
static_assert(!lerEscuro(1000).valido(), "Primeira leitura aguarda confirmacao");
static_assert(!lerEscuro(1299).valido(), "Leitura antes de 300 ms ainda pendente");
static_assert(lerEscuro(1300).valido(), "Leitura confirmada apos 300 ms");
static_assert(lerEscuro(1300).escuro(), "Nivel configurado indica escuro");
static_assert(lerEscuro(1300).brilhoAlvo() == 26, "Escuro reduz PWM para aproximadamente 10 por cento");
static_assert(mudarParaClaro(1799).escuro(), "Transicao preserva estado anterior");
static_assert(!mudarParaClaro(1800).escuro(), "Claro confirmado apos 300 ms");
static_assert(mudarParaClaro(1800).brilhoAlvo() == 255, "Claro restaura brilho maximo");
static_assert(simularOscilacao(1899).escuro(), "Oscilacoes reiniciam confirmacao");
static_assert(!simularOscilacao(1900).escuro(), "Estabilidade apos oscilacao permite mudar");
static_assert(!simularViradaRelogio(171).valido(), "Virada do millis preserva prazo");
static_assert(simularViradaRelogio(172).escuro(), "Virada do millis confirma no prazo");
static_assert(Luminosidade::aproximarBrilho(90, 255) == 95, "Aumento gradual do PWM");
static_assert(Luminosidade::aproximarBrilho(255, 90) == 250, "Reducao gradual do PWM");
static_assert(Luminosidade::aproximarBrilho(253, 255) == 255, "Sem ultrapassar maximo");
static_assert(Luminosidade::aproximarBrilho(92, 90) == 90, "Sem ultrapassar alvo inferior");
static_assert(Luminosidade::aproximarBrilho(90, 90) == 90, "Alvo atingido permanece estavel");
static_assert(Luminosidade::aproximarBrilho(2, 0) == 0, "PWM nao sofre underflow");
static_assert(Luminosidade::aproximarBrilho(0, 255) == 5, "PWM pode iniciar em zero");
