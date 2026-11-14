#pragma once

#include <IPAddress.h>
#include "enderecos_painel.h"

namespace RedePainel {
class Gerenciador {
 public:
  void reiniciar();
  void atualizar(uint32_t agora);
  bool pronto() const { return estado == Estado::Pronto; }
  IPAddress destino() const;
  uint8_t opcaoAtual() const { return opcao < QUANTIDADE_OPCOES ? opcao + 1 : 0; }
  const char *situacao() const;

 private:
  enum class Estado { Inicial, Consultando, Pronto } estado = Estado::Inicial;
  IPAddress ipInicial, gateway, mascara, dns1, dns2, escolhido;
  uint8_t opcao = 0, tentativas = 0;
  uint32_t ultimaConsulta = 0;
  bool houveErroConsulta = false;
  void confirmar(IPAddress ip);
};
}  // namespace RedePainel
