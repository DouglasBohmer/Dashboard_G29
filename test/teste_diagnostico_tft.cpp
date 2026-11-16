#include "resultado_diagnostico_tft.h"
using namespace DiagnosticoTFT;
constexpr Leitura zero{}, ff{255,255,255,255,255,255,255,255};
constexpr Leitura bom{0,0,0x93,0x41,0x9C,0x55,0x28,0xC0};
constexpr Leitura deslocado{0x41,0,0x93,0x41,0x9C,0x55,0x28,0xC0};
constexpr uint8_t outro[] = {0x93,0,0x41};
constexpr Resumo testarResumo() {
  Resumo r;
  r.registrar(zero); r.registrar(zero); r.registrar(zero);
  r.registrar(ff); r.registrar(bom); r.registrar(deslocado);
  return r;
}
constexpr auto resumo = testarResumo();
static_assert(!contem9341(zero.data(),4));
static_assert(!contem9341(ff.data(),4));
static_assert(contem9341(bom.data(),4));
static_assert(contem9341(deslocado.data(),4));
static_assert(!contem9341(outro,3));
static_assert(!contem9341(outro,0));
static_assert(!contem9341(outro,1));
static_assert(uniforme(zero,0) && !uniforme(bom,0));
static_assert(uniforme(ff,255));
static_assert(resumo.amostras == 6);
static_assert(resumo.zeros == 3 && resumo.altos == 1);
static_assert(resumo.compativeis == 2, "Leituras estaveis em zero continuam inconclusivas");
