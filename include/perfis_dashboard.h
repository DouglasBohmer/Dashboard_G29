#pragma once

#include <array>
#include <stdint.h>
#include <stddef.h>

namespace PerfisDashboard {
enum class Jogo : uint8_t { F12025 };
enum class Categoria : uint8_t { F1 };
enum class Pagina : uint8_t { Principal, Temperaturas };

struct Perfil {
  const char *id;
  const char *nome;
  Jogo jogo;
  Categoria categoria;
  Pagina pagina;
  uint16_t formatoUdp;
  uint8_t formula;
};

inline constexpr Perfil PRINCIPAL_F1_2025 = {
    "f1-2025/principal", "Principal F1 2025", Jogo::F12025,
    Categoria::F1, Pagina::Principal, 2025, 0};
// Acrescentar aqui somente telas implementadas e validadas para o respectivo jogo/carro.
inline constexpr Perfil TEMPERATURAS_F1_2025 = {
    "f1-2025/temperaturas", "Pneus e freios F1 2025", Jogo::F12025,
    Categoria::F1, Pagina::Temperaturas, 2025, 0};
inline constexpr std::array<Perfil, 2> DISPONIVEIS = {PRINCIPAL_F1_2025, TEMPERATURAS_F1_2025};
inline constexpr const Perfil &ATIVO = DISPONIVEIS[0];

class Selecao {
 public:
  constexpr const Perfil &atual() const { return DISPONIVEIS[indice]; }
  constexpr void proxima() { indice = (indice + 1) % DISPONIVEIS.size(); }
 private:
  size_t indice = 0;
};

constexpr bool compativel(const Perfil &perfil, uint16_t formato, uint8_t formula) {
  return perfil.formatoUdp == formato && perfil.formula == formula;
}
}  // namespace PerfisDashboard
