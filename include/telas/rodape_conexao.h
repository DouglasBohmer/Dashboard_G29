#pragma once

#include "campos_bufferizados.h"
#include "tela_dashboard.h"
#include "cores_dashboard.h"

namespace InterfaceDashboard {
struct InformacoesConexao {
  const char *etapaDemo = nullptr;
  bool jogoConectado = false;
  String wifi;
  String destino;
};

// Tres linhas de oito pixels preservam o modo/cor ERS e acrescentam a rede.
inline void desenharRodapeConexao(CamposBufferizados &campos,
                                 bool energiaValida, uint8_t modo,
                                 uint32_t agora, const InformacoesConexao &info) {
  const auto estilo = estiloErs(energiaValida, modo);
  String estado = info.etapaDemo ? String("DEMO ") + info.etapaDemo :
                                 String(info.jogoConectado ? "REAL" : "AGUARDANDO JOGO");
  estado += String(" | ERS: ") + (energiaValida ? nomeModo(modo) : "--");
  campos.campo(18, 0, 216, 320, 8, estado, estilo.texto, estilo.fundo);
  // Um SSID tem no maximo 32 bytes; a linha inteira cabe em 320 pixels.
  campos.campo(19, 0, 224, 320, 8, info.wifi, ILI9341_WHITE);
  const bool mostrarCaminho = (agora / 4000) % 2 != 0;
  campos.campo(20, 0, 232, 320, 8,
               mostrarCaminho ? String("F1 25 > Configuracoes > Telemetria (2025)") : info.destino,
               ILI9341_CYAN);
}
}  // namespace InterfaceDashboard
