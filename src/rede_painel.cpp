#include "rede_painel.h"
#include <Arduino.h>
#include <WiFi.h>
#include <lwip/etharp.h>
#include <lwip/priv/tcpip_priv.h>

namespace RedePainel {
namespace {
IPv4 vetor(const IPAddress &ip) { return {ip[0], ip[1], ip[2], ip[3]}; }
IPAddress endereco(const IPv4 &ip) { return IPAddress(ip[0], ip[1], ip[2], ip[3]); }

struct ConsultaArp {
  tcpip_api_call_data chamada{};
  ip4_addr_t origem{}, alvo{};
  bool ocupado = false;
};

// A tabela ARP so e acessada na tarefa de rede do lwIP.
err_t consultarArp(tcpip_api_call_data *chamada) {
  auto &consulta = *reinterpret_cast<ConsultaArp *>(chamada);
  netif *interface = nullptr;
  NETIF_FOREACH(interface) {
    if (!netif_is_up(interface) || !ip4_addr_cmp(netif_ip4_addr(interface), &consulta.origem)) continue;
    eth_addr *mac = nullptr;
    const ip4_addr_t *ip = nullptr;
    consulta.ocupado = etharp_find_addr(interface, &consulta.alvo, &mac, &ip) >= 0;
    return consulta.ocupado ? ERR_OK : etharp_query(interface, &consulta.alvo, nullptr);
  }
  return ERR_IF;
}
}  // namespace

void Gerenciador::reiniciar() {
  estado = Estado::Inicial;
  escolhido = IPAddress();
  opcao = tentativas = 0;
  houveErroConsulta = false;
}

IPAddress Gerenciador::destino() const {
  if (estado == Estado::Pronto) return escolhido;
  if (estado == Estado::Consultando && opcao < QUANTIDADE_OPCOES)
    return endereco(candidato(vetor(ipInicial), vetor(mascara), opcao));
  return endereco(PREFERIDO);
}

const char *Gerenciador::situacao() const {
  if (estado != Estado::Pronto) return "VERIFICANDO IP";
  return opcao < QUANTIDADE_OPCOES ? "" : "IP AUTOMATICO";
}

void Gerenciador::confirmar(IPAddress ip) {
  if (opcao < QUANTIDADE_OPCOES && !WiFi.config(ip, gateway, mascara, dns1, dns2)) {
    Serial.println("Rede: falha ao fixar IP; mantendo configuracao automatica.");
    WiFi.config(IPAddress(), IPAddress(), IPAddress());
    reiniciar();
    return;
  }
  escolhido = ip;
  estado = Estado::Pronto;
  Serial.printf("Rede: destino F1 %s:%u | opcao %u/5.\n",
                escolhido.toString().c_str(), PORTA_UDP, opcaoAtual());
}

void Gerenciador::atualizar(uint32_t agora) {
  if (WiFi.status() != WL_CONNECTED) { reiniciar(); return; }
  if (estado == Estado::Pronto) return;
  if (estado == Estado::Inicial) {
    ipInicial = WiFi.localIP(); gateway = WiFi.gatewayIP(); mascara = WiFi.subnetMask();
    dns1 = WiFi.dnsIP(0); dns2 = WiFi.dnsIP(1);
    if (!uint32_t(ipInicial) || !uint32_t(mascara)) return;
    estado = Estado::Consultando;
    opcao = tentativas = 0;
    ultimaConsulta = agora - 500;
    Serial.printf("Rede detectada: IP temporario=%s | gateway=%s | mascara=%s.\n",
                  ipInicial.toString().c_str(), gateway.toString().c_str(), mascara.toString().c_str());
  }
  if (agora - ultimaConsulta < 500) return;
  ultimaConsulta = agora;
  if (opcao >= QUANTIDADE_OPCOES) {
    Serial.println("Rede: sem opcao fixa confirmada; usando o IP concedido pelo roteador.");
    confirmar(ipInicial);
    return;
  }
  const auto candidatoAtual = candidato(vetor(ipInicial), vetor(mascara), opcao);
  const IPAddress ip = endereco(candidatoAtual);
  if (!uint32_t(ip) || ip == gateway) { ++opcao; tentativas = 0; return; }
  if (ip == ipInicial) { confirmar(ip); return; }
  ConsultaArp consulta;
  consulta.origem.addr = uint32_t(ipInicial);
  consulta.alvo.addr = uint32_t(ip);
  const err_t resultado = tcpip_api_call(consultarArp, &consulta.chamada);
  houveErroConsulta |= resultado != ERR_OK;
  if (consulta.ocupado) {
    Serial.printf("Rede: IP %s ocupado; tentando a proxima opcao.\n", ip.toString().c_str());
    ++opcao; tentativas = 0; houveErroConsulta = false;
  } else if (++tentativas >= 4) {
    if (houveErroConsulta) { ++opcao; tentativas = 0; houveErroConsulta = false; }
    else confirmar(ip);
  }
}
}  // namespace RedePainel
