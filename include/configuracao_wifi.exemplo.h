#pragma once

#include <stddef.h>

namespace ConfiguracaoWiFi {
struct Rede {
  const char *nome;
  const char *senha;
  bool permitirWpaLegado = false;  // Ative somente para redes WPA antigas.
};

// Copie para configuracao_wifi.local.h e preencha as redes de 2,4 GHz.
constexpr Rede REDES[] = {
    {"", ""},  // Casa.
    {"", ""},  // Faculdade, se usar senha comum e permitir acesso local.
};
constexpr size_t QUANTIDADE_REDES = sizeof(REDES) / sizeof(REDES[0]);
}  // namespace ConfiguracaoWiFi
