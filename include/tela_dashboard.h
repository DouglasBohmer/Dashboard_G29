#pragma once

#include <Arduino.h>
#include "dashboard_f1.h"
#include "enderecos_painel.h"

// Formatacao compartilhada. Cada pagina desenha seu layout em include/telas/.
namespace InterfaceDashboard {
inline String formatarDestinoF1(const RedePainel::IPv4 &ip, uint8_t opcao) {
  char texto[40];
  if (opcao) snprintf(texto, sizeof(texto), "%u/5 F1>%u.%u.%u.%u:%u", opcao,
                      ip[0], ip[1], ip[2], ip[3], RedePainel::PORTA_UDP);
  else texto[0] = '\0';
  if (!opcao || std::strlen(texto) > 25) {
    snprintf(texto, sizeof(texto), "F1>%u.%u.%u.%u:%u",
             ip[0], ip[1], ip[2], ip[3], RedePainel::PORTA_UDP);
  }
  return texto;
}

inline String formatarSensores(uint8_t brilho, bool ambienteValido,
                              float temperatura, float umidade) {
  char texto[40];
  const unsigned percentual = (brilho * 100U + 127U) / 255U;
  if (ambienteValido && std::isfinite(temperatura) && std::isfinite(umidade)) {
    snprintf(texto, sizeof(texto), "Luz:%u%% T:%.1fC U:%.1f%%",
             percentual, temperatura, umidade);
  } else snprintf(texto, sizeof(texto), "Luz:%u%% T:-- U:--", percentual);
  return texto;
}

inline String formatarTempo(uint32_t tempo) {
  if (!tempo || tempo >= 86400000UL) return "--:--.---";
  char texto[20];
  snprintf(texto, sizeof(texto), "%lu:%02lu.%03lu",
           static_cast<unsigned long>(tempo / 60000),
           static_cast<unsigned long>((tempo / 1000) % 60),
           static_cast<unsigned long>(tempo % 1000));
  return texto;
}

inline String formatarIntervalo(const DashboardF1::Intervalo &intervalo, char sinal = '+') {
  if (!intervalo.valido) return "--";
  if (intervalo.voltas) return String(sinal) + String(intervalo.voltas) + "V";
  char texto[24];
  snprintf(texto, sizeof(texto), "%c%lu.%03lu s", sinal,
           static_cast<unsigned long>(intervalo.milissegundos / 1000),
           static_cast<unsigned long>(intervalo.milissegundos % 1000));
  return texto;
}

inline String formatarDelta(const DashboardF1::DeltaVolta &delta) {
  if (!delta.valido) return "--";
  const int64_t valor = delta.milissegundos;
  const uint32_t modulo = valor < 0 ? -valor : valor;
  char texto[24];
  snprintf(texto, sizeof(texto), "%c%lu.%03lu", valor < 0 ? '-' : '+',
           static_cast<unsigned long>(modulo / 1000),
           static_cast<unsigned long>(modulo % 1000));
  return texto;
}

// O display usa fonte de um byte. Converter acentos latinos do nome UTF-8.
inline String nomeExibivel(const char *origem) {
  String nome;
  for (size_t i = 0; origem[i]; ++i) {
    const uint8_t c = static_cast<uint8_t>(origem[i]);
    if (c >= 32 && c < 127) nome += static_cast<char>(c);
    else if (c == 0xC3 && origem[i + 1]) {
      const uint8_t acento = static_cast<uint8_t>(origem[++i]);
      const char *letras = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";
      nome += acento >= 0x80 && acento <= 0xBF ? letras[acento - 0x80] : '?';
    } else if ((c & 0xC0) != 0x80) nome += '?';
  }
  nome.trim();
  return nome;
}

inline String formatarVizinho(const DashboardF1::Telemetria &dados, bool frente) {
  const uint8_t indice = frente ? dados.volta.indiceFrente : dados.volta.indiceAtras;
  if (indice >= F1::QUANTIDADE_CARROS || !dados.volta.posicao) return "--";
  const uint8_t posicao = frente ? dados.volta.posicao - 1 : dados.volta.posicao + 1;
  String nome = nomeExibivel(dados.nomePiloto(indice));
  if (dados.pilotoIa(indice)) {
    const int espaco = nome.lastIndexOf(' ');
    if (espaco >= 0) nome = nome.substring(espaco + 1);
  }
  if (!nome.length()) nome = "--";
  String texto = String(posicao) + String(static_cast<char>(0xA7)) + " - " + nome;
  // 16 caracteres x 6 pixels cabem nos dois campos de 100/101 pixels.
  if (texto.length() > 16) texto = texto.substring(0, 13) + "...";
  return texto;
}

inline const char *nomeModo(uint8_t modo) {
  switch (modo) {
    case 0: return "NENHUM";
    case 1: return "MEDIO";
    case 2: return "VOLTA RAPIDA";
    case 3: return "ULTRAPASSAGEM";
    default: return "--";
  }
}
}  // namespace InterfaceDashboard
