#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

#include "configuracao_tela.h"
#include "telemetria_f1.h"

#if __has_include("configuracao_wifi.local.h")
#include "configuracao_wifi.local.h"
#else
#include "configuracao_wifi.exemplo.h"
#endif

namespace Configuracao {
constexpr uint16_t PORTA_UDP = 20777;
constexpr uint32_t TEMPO_TENTATIVA_WIFI = 15000;
constexpr uint32_t TEMPO_SEM_TELEMETRIA = 3000;
constexpr uint32_t INTERVALO_TELA = 200;
constexpr size_t TAMANHO_BUFFER = 1536;
}  // namespace Configuracao

Adafruit_ILI9341 tela(Configuracao::PINO_CS, Configuracao::PINO_DC,
                      Configuracao::PINO_RESET);
WiFiUDP receptor;
F1::DadosCarro dadosCarro;
uint8_t pacoteRecebido[Configuracao::TAMANHO_BUFFER];

bool haRedesConfiguradas = false;
bool tentandoWiFi = false;
bool udpAtivo = false;
bool temTelemetria = false;
size_t proximaRede = 0;
const char *nomeRedeEmTeste = "";
IPAddress ipAtivo;
IPAddress origemTelemetria;
uint32_t inicioTentativa = 0;
uint32_t ultimaTentativaUdp = 0;
uint32_t ultimaTelemetria = 0;
uint32_t ultimoErro = 0;
uint32_t ultimaTela = 0;
uint32_t ultimoRelatorio = 0;
uint32_t pacotesRecebidos = 0;
uint32_t pacotesTelemetria = 0;
uint32_t pacotesDescartados = 0;
uint32_t pacotesForaDeOrdem = 0;
uint16_t formatoRecebido = 0;
F1::Resultado erroProtocolo = F1::Resultado::OutroTipo;
String camposAnteriores[8];

void registrarEventoWiFi(WiFiEvent_t evento, WiFiEventInfo_t informacao) {
  if (evento != ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    return;
  }
  const uint8_t motivo = informacao.wifi_sta_disconnected.reason;
  const char *descricao = "consulte o codigo de desconexao";
  switch (motivo) {
    case WIFI_REASON_NO_AP_FOUND:
      descricao = "rede nao encontrada ou recusada pelo filtro de seguranca";
      break;
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_AUTH_EXPIRE:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
      descricao = "falha de autenticacao; confira senha e seguranca do roteador";
      break;
    case WIFI_REASON_ASSOC_LEAVE:
      descricao = "conexao encerrada para uma nova tentativa";
      break;
    case WIFI_REASON_BEACON_TIMEOUT:
      descricao = "roteador deixou de responder; confira sinal e alimentacao";
      break;
  }
  // O evento ocorre em outra tarefa; somente registrar, sem alterar a tela.
  Serial.printf("Wi-Fi: motivo %u (%s).\n", motivo, descricao);
}

bool telemetriaRecente(uint32_t agora) {
  return udpAtivo && temTelemetria &&
         agora - ultimaTelemetria < Configuracao::TEMPO_SEM_TELEMETRIA;
}

void encerrarRecepcao() {
  receptor.stop();
  udpAtivo = false;
  temTelemetria = false;
  erroProtocolo = F1::Resultado::OutroTipo;
}

void atualizarConexao(uint32_t agora) {
  if (!haRedesConfiguradas) {
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    tentandoWiFi = false;
    if (udpAtivo && WiFi.localIP() != ipAtivo) {
      encerrarRecepcao();
    }
    if (!udpAtivo && agora - ultimaTentativaUdp >= 2000) {
      ultimaTentativaUdp = agora;
      udpAtivo = receptor.begin(Configuracao::PORTA_UDP) == 1;
      if (udpAtivo) {
        ipAtivo = WiFi.localIP();
        Serial.printf("\nWi-Fi conectado: %s\n", WiFi.SSID().c_str());
        Serial.printf("Destino no F1: IP %s, UDP %u, formato 2025.\n",
                      ipAtivo.toString().c_str(), Configuracao::PORTA_UDP);
      } else {
        Serial.println("Falha ao abrir UDP; nova tentativa em 2 segundos.");
      }
    }
    return;
  }

  if (udpAtivo) {
    Serial.println("Wi-Fi desconectado; suspendendo os dados exibidos.");
    encerrarRecepcao();
  }
  if (tentandoWiFi &&
      agora - inicioTentativa < Configuracao::TEMPO_TENTATIVA_WIFI) {
    return;
  }

  WiFi.disconnect();
  for (size_t tentativa = 0; tentativa < ConfiguracaoWiFi::QUANTIDADE_REDES;
       ++tentativa) {
    const auto &rede = ConfiguracaoWiFi::REDES[proximaRede];
    proximaRede = (proximaRede + 1) % ConfiguracaoWiFi::QUANTIDADE_REDES;
    if (rede.nome[0] == '\0') {
      continue;
    }
    nomeRedeEmTeste = rede.nome;
    inicioTentativa = agora;
    tentandoWiFi = true;
    Serial.printf("Tentando Wi-Fi: %s\n", nomeRedeEmTeste);
    WiFi.setMinSecurity(rede.permitirWpaLegado ? WIFI_AUTH_WPA_PSK
                                            : WIFI_AUTH_WPA2_PSK);
    Serial.printf("Seguranca minima: %s.\n",
                  rede.permitirWpaLegado ? "WPA (compatibilidade explicita)"
                                        : "WPA2");
    if (WiFi.begin(rede.nome, rede.senha) == WL_CONNECT_FAILED) {
      Serial.println("Nao foi possivel iniciar a conexao Wi-Fi.");
    }
    return;
  }
}

void receberTelemetria(uint32_t agora) {
  if (!udpAtivo) {
    return;
  }
  for (uint8_t quantidade = 0; quantidade < 20; ++quantidade) {
    const int tamanho = receptor.parsePacket();
    if (tamanho <= 0) {
      break;
    }
    ++pacotesRecebidos;
    if (static_cast<size_t>(tamanho) > sizeof(pacoteRecebido)) {
      receptor.flush();
      ++pacotesDescartados;
      continue;
    }
    const IPAddress origem = receptor.remoteIP();
    const int lidos = receptor.read(pacoteRecebido, sizeof(pacoteRecebido));
    receptor.flush();
    if (lidos != tamanho) {
      ++pacotesDescartados;
      continue;
    }
    if (lidos >= 2) {
      formatoRecebido = F1::ler16(pacoteRecebido);
    }

    const auto interpretado =
        F1::interpretar(pacoteRecebido, static_cast<size_t>(lidos));
    if (interpretado.resultado == F1::Resultado::OutroTipo) {
      continue;
    }
    if (interpretado.resultado != F1::Resultado::Telemetria) {
      ++pacotesDescartados;
      erroProtocolo = interpretado.resultado;
      ultimoErro = agora;
      continue;
    }

    const auto &novosDados = interpretado.dados;
    if (temTelemetria && novosDados.sessao == dadosCarro.sessao &&
        !F1::quadroMaisRecente(novosDados.quadro, dadosCarro.quadro)) {
      ++pacotesForaDeOrdem;
      continue;
    }
    dadosCarro = novosDados;
    origemTelemetria = origem;
    ultimaTelemetria = agora;
    temTelemetria = true;
    ++pacotesTelemetria;
  }
}

void escreverCampo(uint8_t indice, int16_t x, int16_t y, int16_t largura,
                   int16_t altura, uint8_t tamanho, uint16_t cor,
                   const String &texto) {
  if (camposAnteriores[indice] == texto) {
    return;
  }
  camposAnteriores[indice] = texto;
  tela.fillRect(x, y, largura, altura, ILI9341_BLACK);
  tela.setTextSize(tamanho);
  tela.setTextColor(cor, ILI9341_BLACK);
  tela.setCursor(x, y);
  tela.print(texto);
}

void prepararTela() {
  tela.setRotation(1);
  tela.setTextWrap(false);
  tela.fillScreen(ILI9341_BLACK);
  tela.drawRect(0, 0, 320, 240, ILI9341_DARKGREY);
  tela.setTextColor(ILI9341_CYAN);
  tela.setTextSize(2);
  tela.setCursor(8, 10);
  tela.print("F1 25 | TESTE DE REDE");
  tela.drawFastHLine(8, 120, 304, ILI9341_DARKGREY);
  tela.setTextSize(1);
  tela.setTextColor(ILI9341_WHITE);
  tela.setCursor(12, 130);
  tela.print("VELOCIDADE");
  tela.setCursor(174, 130);
  tela.print("RPM");
  tela.setCursor(12, 184);
  tela.print("km/h");
}

void atualizarTela(uint32_t agora) {
  if (agora - ultimaTela < Configuracao::INTERVALO_TELA) {
    return;
  }
  ultimaTela = agora;
  const bool conectado = WiFi.status() == WL_CONNECTED;
  const bool dadosAtuais = telemetriaRecente(agora);
  String estado;
  uint16_t corEstado = ILI9341_YELLOW;
  if (!haRedesConfiguradas) {
    estado = "CONFIGURE O WI-FI";
  } else if (!conectado) {
    estado = "CONECTANDO WI-FI";
  } else if (!udpAtivo) {
    estado = "ABRINDO PORTA UDP";
  } else if (dadosAtuais) {
    estado = "TELEMETRIA ATIVA";
    corEstado = ILI9341_GREEN;
  } else if (agora - ultimoErro < 3000 &&
             erroProtocolo == F1::Resultado::FormatoIncompativel) {
    estado = "SELECIONE UDP 2025";
    corEstado = ILI9341_RED;
  } else if (agora - ultimoErro < 3000 &&
             erroProtocolo == F1::Resultado::VersaoIncompativel) {
    estado = "VERSAO UDP INCOMPATIVEL";
    corEstado = ILI9341_RED;
  } else {
    estado = temTelemetria ? "SINAL DE F1 AUSENTE" : "AGUARDANDO F1 25";
  }
  escreverCampo(0, 8, 38, 304, 20, 2, corEstado, estado);
  escreverCampo(1, 8, 65, 304, 20, 2, ILI9341_WHITE,
                 conectado ? "IP: " + WiFi.localIP().toString() : "IP: --");
  escreverCampo(2, 8, 93, 304, 10, 1, ILI9341_WHITE,
                 "WiFi: " + (conectado ? WiFi.SSID() : String(nomeRedeEmTeste)));
  escreverCampo(3, 8, 107, 304, 10, 1, ILI9341_WHITE,
                 String("UDP 20777 | formato recebido: ") + formatoRecebido);
  escreverCampo(4, 12, 146, 140, 34, 4, ILI9341_WHITE,
                 dadosAtuais ? String(dadosCarro.velocidadeKmh) : "--");
  escreverCampo(5, 174, 146, 140, 34, 4, ILI9341_WHITE,
                 dadosAtuais ? String(dadosCarro.rpm) : "--");
  escreverCampo(6, 8, 201, 304, 10, 1, ILI9341_WHITE,
                 "UDP: " + String(pacotesRecebidos) +
                     " | telemetria: " + String(pacotesTelemetria));
  escreverCampo(7, 8, 222, 304, 10, 1, ILI9341_YELLOW,
                 haRedesConfiguradas
                     ? "PS5 cabo ou WiFi: mesma rede local"
                     : "Edite configuracao_wifi.local.h");
}

void relatarDados(uint32_t agora) {
  Serial.printf("WiFi=%s | IP=%s | UDP=%lu | telemetria=%lu | descartados=%lu | "
                "atrasados=%lu | formato=%u\n",
                WiFi.status() == WL_CONNECTED ? "conectado" : "desconectado",
                WiFi.localIP().toString().c_str(),
                static_cast<unsigned long>(pacotesRecebidos),
                static_cast<unsigned long>(pacotesTelemetria),
                static_cast<unsigned long>(pacotesDescartados),
                static_cast<unsigned long>(pacotesForaDeOrdem), formatoRecebido);
  if (telemetriaRecente(agora)) {
    Serial.printf("Origem=%s | carro=%u | velocidade=%u km/h | RPM=%u | "
                  "marcha=%d | luzesRPM=%u%%\n",
                  origemTelemetria.toString().c_str(), dadosCarro.indiceCarro,
                  dadosCarro.velocidadeKmh, dadosCarro.rpm, dadosCarro.marcha,
                  dadosCarro.luzesRpm);
  }
}

void setup() {
  // Manter o resistor de 220 ou 300 ohms entre GPIO32 e LED da TFT.
  pinMode(Configuracao::PINO_BACKLIGHT, OUTPUT);
  digitalWrite(Configuracao::PINO_BACKLIGHT, HIGH);
  Serial.begin(115200);
  SPI.begin(Configuracao::PINO_SCK, Configuracao::PINO_MISO,
            Configuracao::PINO_MOSI, Configuracao::PINO_CS);
  tela.begin(Configuracao::FREQUENCIA_SPI);
  prepararTela();

  for (const auto &rede : ConfiguracaoWiFi::REDES) {
    haRedesConfiguradas = haRedesConfiguradas || rede.nome[0] != '\0';
  }
  WiFi.persistent(false);
  WiFi.onEvent(registrarEventoWiFi);
  WiFi.setHostname("dashboard-g29");
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);
  WiFi.setSleep(false);
  Serial.println("\n=== F1 25: PS5 POR CABO OU WI-FI ===");
  Serial.println("O ESP32 sempre usa Wi-Fi de 2,4 GHz; UDP 20777, formato 2025.");
  Serial.println("Comandos: i = informacoes; r = tentar conexao novamente.");
  if (!haRedesConfiguradas) {
    Serial.println("Preencha include/configuracao_wifi.local.h e grave novamente.");
  }
}

void loop() {
  const uint32_t agora = millis();
  atualizarConexao(agora);
  receberTelemetria(agora);
  atualizarTela(agora);

  while (Serial.available() > 0) {
    const char comando = static_cast<char>(Serial.read());
    if (comando == 'i' || comando == 'I') {
      relatarDados(agora);
    } else if (comando == 'r' || comando == 'R') {
      encerrarRecepcao();
      WiFi.disconnect();
      tentandoWiFi = false;
    }
  }
  if (agora - ultimoRelatorio >= 2000) {
    ultimoRelatorio = agora;
    relatarDados(agora);
  }
  delay(2);
}
