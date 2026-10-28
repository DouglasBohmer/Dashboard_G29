#include "controle_adafruit.h"

constexpr bool primeiraTentativaImediata() {
  AdafruitIO::Temporizador temporizador;
  return temporizador.consumir(0, AdafruitIO::INTERVALO_ENVIO_MS);
}

constexpr bool respeitarIntervalo(uint32_t instante) {
  AdafruitIO::Temporizador temporizador;
  temporizador.consumir(0, AdafruitIO::INTERVALO_ENVIO_MS);
  return temporizador.consumir(instante, AdafruitIO::INTERVALO_ENVIO_MS);
}

constexpr unsigned contarPontosEmUmMinuto() {
  AdafruitIO::Temporizador temporizador;
  unsigned pontos = 0;
  for (uint32_t agora = 0; agora < 60000; agora += 100) {
    if (temporizador.consumir(agora, AdafruitIO::INTERVALO_ENVIO_MS)) pontos += 2;
  }
  return pontos;
}

constexpr bool intervaloAposReconexao() {
  AdafruitIO::Temporizador temporizador;
  const bool primeiro = temporizador.consumir(0, AdafruitIO::INTERVALO_ENVIO_MS);
  const bool reconectouCedo = temporizador.consumir(3000, AdafruitIO::INTERVALO_ENVIO_MS);
  const bool reconectouDepois = temporizador.consumir(45000, AdafruitIO::INTERVALO_ENVIO_MS);
  const bool repetiu = temporizador.consumir(45000, AdafruitIO::INTERVALO_ENVIO_MS);
  return primeiro && !reconectouCedo && reconectouDepois && !repetiu;
}

constexpr bool contarNaVirada(uint32_t instante) {
  AdafruitIO::Temporizador temporizador;
  temporizador.consumir(0xFFFFFF00U, AdafruitIO::INTERVALO_ENVIO_MS);
  return temporizador.consumir(instante, AdafruitIO::INTERVALO_ENVIO_MS);
}

constexpr bool reconexaoLimitada() {
  AdafruitIO::Temporizador temporizador;
  unsigned tentativas = 0;
  for (uint32_t agora = 0; agora < 60000; agora += 100) {
    if (temporizador.consumir(agora, AdafruitIO::INTERVALO_RECONEXAO_MS)) ++tentativas;
  }
  return tentativas <= 6;
}

static_assert(primeiraTentativaImediata(), "Primeira amostra pode ser enviada imediatamente");
static_assert(!respeitarIntervalo(0), "Nao repetir envio no instante inicial");
static_assert(!respeitarIntervalo(3000), "Dois feeds nao devem repetir a cada 3 segundos");
static_assert(!respeitarIntervalo(9999), "Aguardar intervalo completo");
static_assert(respeitarIntervalo(10000), "Enviar ao completar 10 segundos");
static_assert(contarPontosEmUmMinuto() == 12, "Dois feeds geram 12 pontos por minuto");
static_assert(contarPontosEmUmMinuto() <= 30, "Respeitar limite gratuito da conta");
static_assert(intervaloAposReconexao(), "Reconectar nao reinicia intervalo nem cria rajada");
static_assert(!contarNaVirada(9743), "Virada do millis preserva intervalo antes do prazo");
static_assert(contarNaVirada(9744), "Virada do millis libera envio no prazo");
static_assert(reconexaoLimitada(), "Limitar tentativas mesmo com falhas repetidas");
static_assert(AdafruitIO::percentualBrilho(0) == 0, "PWM zero representa zero por cento");
static_assert(AdafruitIO::percentualBrilho(26) == 10, "Brilho no escuro representa 10 por cento");
static_assert(AdafruitIO::percentualBrilho(128) == 50, "Meio ciclo representa 50 por cento");
static_assert(AdafruitIO::percentualBrilho(255) == 100, "Ciclo completo representa 100 por cento");
