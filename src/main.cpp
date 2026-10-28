#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Adafruit_ILI9341.h>
#include "configuracao_tela.h"
#include "dashboard_f1.h"
#include "tela_dashboard.h"
#include "sensor_luminosidade.h"
#include "integracao_adafruit.h"

#if __has_include("configuracao_wifi.local.h")
#include "configuracao_wifi.local.h"
#else
#include "configuracao_wifi.exemplo.h"
#endif

namespace Configuracao {
constexpr uint16_t PORTA_UDP = 20777;
constexpr uint32_t TEMPO_TENTATIVA_WIFI = 15000;
constexpr uint32_t INTERVALO_TELA = 33;
constexpr size_t TAMANHO_BUFFER = 1536;
}

Adafruit_ILI9341 tela(Configuracao::PINO_CS, Configuracao::PINO_DC,
                      Configuracao::PINO_RESET);
InterfaceDashboard::Tela painel(tela);
DashboardF1::Telemetria dados;
Luminosidade::Sensor sensorLuz;
AdafruitIO::Integracao nuvem;
bool pwmDisponivel = false;
uint8_t brilhoAtual = ConfiguracaoLuminosidade::BRILHO_CLARO;
uint32_t ultimaLeituraLuz = 0, ultimoAjusteBrilho = 0;
WiFiUDP receptor;
uint8_t pacoteRecebido[Configuracao::TAMANHO_BUFFER];
bool haRedesConfiguradas = false, tentandoWiFi = false, udpAtivo = false;
size_t proximaRede = 0;
const char *nomeRedeEmTeste = "";
IPAddress ipAtivo, origemTelemetria;
uint32_t inicioTentativa = 0, ultimaTentativaUdp = 0, ultimaTela = 0;
uint32_t ultimoRelatorio = 0, ultimoErro = 0, maiorTempoDesenhoUs = 0;
uint32_t pacotesRecebidos = 0, pacotesAtualizados = 0;
uint32_t pacotesDescartados = 0, pacotesForaDeOrdem = 0;
uint32_t pacotesPorTipo[16]{};
uint16_t formatoRecebido = 0;
DashboardF1::Resultado erroProtocolo = DashboardF1::Resultado::Ignorado;

void iniciarLuminosidade() {
  pinMode(ConfiguracaoLuminosidade::PINO_SENSOR, INPUT);
  // Manter o resistor de 220 ou 300 ohms entre GPIO32 e LED da TFT.
  pinMode(Configuracao::PINO_BACKLIGHT, OUTPUT);
  digitalWrite(Configuracao::PINO_BACKLIGHT, HIGH);
  pwmDisponivel = ledcSetup(ConfiguracaoLuminosidade::CANAL_PWM,
                            ConfiguracaoLuminosidade::FREQUENCIA_PWM_HZ,
                            ConfiguracaoLuminosidade::RESOLUCAO_PWM_BITS) > 0;
  if (pwmDisponivel) {
    ledcAttachPin(Configuracao::PINO_BACKLIGHT, ConfiguracaoLuminosidade::CANAL_PWM);
    ledcWrite(ConfiguracaoLuminosidade::CANAL_PWM, brilhoAtual);
  } else Serial.println("Falha no PWM; backlight mantido aceso.");
  sensorLuz.atualizar(digitalRead(ConfiguracaoLuminosidade::PINO_SENSOR) == HIGH, millis());
}

void atualizarLuminosidade(uint32_t agora) {
  if (agora - ultimaLeituraLuz >= ConfiguracaoLuminosidade::INTERVALO_LEITURA_MS) {
    ultimaLeituraLuz = agora;
    sensorLuz.atualizar(digitalRead(ConfiguracaoLuminosidade::PINO_SENSOR) == HIGH, agora);
  }
  if (!pwmDisponivel || agora - ultimoAjusteBrilho < ConfiguracaoLuminosidade::INTERVALO_AJUSTE_MS) return;
  ultimoAjusteBrilho = agora;
  const uint8_t novoBrilho = Luminosidade::aproximarBrilho(brilhoAtual, sensorLuz.brilhoAlvo());
  if (novoBrilho == brilhoAtual) return;
  brilhoAtual = novoBrilho;
  ledcWrite(ConfiguracaoLuminosidade::CANAL_PWM, brilhoAtual);
}

void registrarEventoWiFi(WiFiEvent_t evento, WiFiEventInfo_t informacao) {
  if (evento != ARDUINO_EVENT_WIFI_STA_DISCONNECTED) return;
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
  Serial.printf("Wi-Fi: motivo %u (%s).\n", motivo, descricao);
}

void encerrarRecepcao() {
  receptor.stop();
  udpAtivo = false;
  dados.reiniciar();
  erroProtocolo = DashboardF1::Resultado::Ignorado;
}

void atualizarConexao(uint32_t agora) {
  if (!haRedesConfiguradas) return;
  if (WiFi.status() == WL_CONNECTED) {
    tentandoWiFi = false;
    if (udpAtivo && WiFi.localIP() != ipAtivo) encerrarRecepcao();
    if (!udpAtivo && agora - ultimaTentativaUdp >= 2000) {
      ultimaTentativaUdp = agora;
      udpAtivo = receptor.begin(Configuracao::PORTA_UDP) == 1;
      if (udpAtivo) {
        ipAtivo = WiFi.localIP();
        Serial.printf("\nWi-Fi conectado: %s\n", WiFi.SSID().c_str());
        Serial.printf("Destino no F1: IP %s, UDP %u, formato 2025.\n",
                      ipAtivo.toString().c_str(), Configuracao::PORTA_UDP);
      } else Serial.println("Falha ao abrir UDP; nova tentativa em 2 segundos.");
    }
    return;
  }
  if (udpAtivo) {
    Serial.println("Wi-Fi desconectado; suspendendo os dados exibidos.");
    encerrarRecepcao();
  }
  if (tentandoWiFi && agora - inicioTentativa < Configuracao::TEMPO_TENTATIVA_WIFI) return;
  WiFi.disconnect();
  for (size_t tentativa = 0; tentativa < ConfiguracaoWiFi::QUANTIDADE_REDES; ++tentativa) {
    const auto &rede = ConfiguracaoWiFi::REDES[proximaRede];
    proximaRede = (proximaRede + 1) % ConfiguracaoWiFi::QUANTIDADE_REDES;
    if (rede.nome[0] == '\0') continue;
    nomeRedeEmTeste = rede.nome;
    inicioTentativa = agora;
    tentandoWiFi = true;
    WiFi.setMinSecurity(rede.permitirWpaLegado ? WIFI_AUTH_WPA_PSK : WIFI_AUTH_WPA2_PSK);
    Serial.printf("Tentando Wi-Fi: %s | seguranca minima: %s\n", nomeRedeEmTeste,
                  rede.permitirWpaLegado ? "WPA (compatibilidade explicita)" : "WPA2");
    if (WiFi.begin(rede.nome, rede.senha) == WL_CONNECT_FAILED) {
      Serial.println("Nao foi possivel iniciar a conexao Wi-Fi.");
    }
    return;
  }
}

void receberTelemetria(uint32_t agora) {
  if (!udpAtivo) return;
  for (uint8_t quantidade = 0; quantidade < 40; ++quantidade) {
    const int tamanho = receptor.parsePacket();
    if (tamanho <= 0) break;
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
    if (lidos >= 2) formatoRecebido = F1::ler16(pacoteRecebido);
    const auto resultado = dados.processar(pacoteRecebido, lidos, agora);
    if (resultado == DashboardF1::Resultado::Ignorado) continue;
    if (resultado == DashboardF1::Resultado::ForaDeOrdem) {
      ++pacotesForaDeOrdem;
      continue;
    }
    if (resultado != DashboardF1::Resultado::Atualizado) {
      ++pacotesDescartados;
      erroProtocolo = resultado;
      ultimoErro = agora;
      continue;
    }
    ++pacotesAtualizados;
    ++pacotesPorTipo[pacoteRecebido[6]];
    origemTelemetria = origem;
  }
}

void atualizarTela(uint32_t agora) {
  if (agora - ultimaTela < Configuracao::INTERVALO_TELA) return;
  ultimaTela = agora;
  const uint32_t inicio = micros();
  const bool conectado = WiFi.status() == WL_CONNECTED;
  String situacao;
  if (!haRedesConfiguradas) situacao = "CONFIGURE O WI-FI NO ARQUIVO LOCAL";
  else if (!conectado) situacao = "CONECTANDO WI-FI: " + String(nomeRedeEmTeste);
  else if (!udpAtivo) situacao = "ABRINDO UDP | " + WiFi.localIP().toString();
  else if (agora - ultimoErro < 3000 && erroProtocolo == DashboardF1::Resultado::FormatoIncompativel) {
    situacao = "SELECIONE UDP 2025 NO F1";
  } else if (agora - ultimoErro < 3000 && erroProtocolo == DashboardF1::Resultado::VersaoIncompativel) {
    situacao = "VERSAO UDP INCOMPATIVEL";
  } else {
    situacao = String(dados.carroRecente(agora) ? "F1 ATIVO | " : "AGUARDANDO F1 | ") + WiFi.localIP().toString();
    situacao += String(" | LUZ:") + (!sensorLuz.valido() ? "--" : sensorLuz.escuro() ? "ESC" : "CLA") +
                " B" + String((brilhoAtual * 100U + 127U) / 255U) + "%";
  }
  painel.atualizar(dados, agora, situacao, conectado && udpAtivo);
  const uint32_t tempo = micros() - inicio;
  if (tempo > maiorTempoDesenhoUs) maiorTempoDesenhoUs = tempo;
}

void relatarDados(uint32_t agora) {
  Serial.printf("WiFi=%s | IP=%s | UDP=%lu | atualizados=%lu | descartados=%lu | atrasados=%lu | formato=%u\n",
                WiFi.status() == WL_CONNECTED ? "conectado" : "desconectado",
                WiFi.localIP().toString().c_str(), static_cast<unsigned long>(pacotesRecebidos),
                static_cast<unsigned long>(pacotesAtualizados), static_cast<unsigned long>(pacotesDescartados),
                static_cast<unsigned long>(pacotesForaDeOrdem), formatoRecebido);
  Serial.printf("Pacotes: sessao=%lu | voltas=%lu | carro=%lu | energia=%lu | historico=%lu\n",
                static_cast<unsigned long>(pacotesPorTipo[1]), static_cast<unsigned long>(pacotesPorTipo[2]),
                static_cast<unsigned long>(pacotesPorTipo[6]), static_cast<unsigned long>(pacotesPorTipo[7]),
                static_cast<unsigned long>(pacotesPorTipo[11]));
  Serial.printf("Tela: alvo=%lu ms | maior desenho=%lu us | heap livre=%u\n",
                static_cast<unsigned long>(Configuracao::INTERVALO_TELA),
                static_cast<unsigned long>(maiorTempoDesenhoUs), ESP.getFreeHeap());
  maiorTempoDesenhoUs = 0;
  Serial.printf("Luz: DO=%u | ambiente=%s | brilho PWM=%u%% | PWM=%s\n",
                digitalRead(ConfiguracaoLuminosidade::PINO_SENSOR),
                !sensorLuz.valido() ? "estabilizando" : sensorLuz.escuro() ? "escuro" : "claro",
                (brilhoAtual * 100U + 127U) / 255U, pwmDisponivel ? "ativo" : "indisponivel");
  nuvem.relatar();
  if (dados.carroRecente(agora)) {
    Serial.printf("Origem=%s | velocidade=%u | RPM=%u | marcha=%d | DRS=%u | luzes=%u%%\n",
                  origemTelemetria.toString().c_str(), dados.carro.velocidadeKmh, dados.carro.rpm,
                  dados.carro.marcha, dados.carro.drsAtivo, dados.carro.luzesRpm);
  }
  if (dados.voltaRecente(agora)) {
    Serial.printf("P%u | volta=%u/%u | tempo=%s | frente=%s | atras=%s | minha=%s | prova=%s\n",
                  dados.volta.posicao, dados.volta.numero, dados.totalVoltas,
                  InterfaceDashboard::formatarTempo(dados.volta.tempo).c_str(),
                  InterfaceDashboard::formatarIntervalo(dados.volta.frente).c_str(),
                  InterfaceDashboard::formatarIntervalo(dados.volta.atras).c_str(),
                  InterfaceDashboard::formatarTempo(dados.minhaMelhor(agora)).c_str(),
                  InterfaceDashboard::formatarTempo(dados.melhorProva(agora)).c_str());
  }
  if (dados.energiaRecente(agora)) {
    Serial.printf("ERS: bateria=%.0f J | usado na volta=%.0f J | modo=%s | formula=%u\n",
                  dados.energia.armazenada, dados.energia.aplicadaNaVolta,
                  InterfaceDashboard::nomeModo(dados.energia.modo), dados.formula);
  }
}

void setup() {
  Serial.begin(115200);
  iniciarLuminosidade();
  if (!painel.memoriaDisponivel()) Serial.println("Falha ao reservar buffer de desenho da TFT.");
  SPI.begin(Configuracao::PINO_SCK, Configuracao::PINO_MISO,
            Configuracao::PINO_MOSI, Configuracao::PINO_CS);
  tela.begin(Configuracao::FREQUENCIA_SPI);
  painel.iniciar();
  for (const auto &rede : ConfiguracaoWiFi::REDES) haRedesConfiguradas |= rede.nome[0] != '\0';
  WiFi.persistent(false);
  WiFi.onEvent(registrarEventoWiFi);
  WiFi.setHostname("dashboard-g29");
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);
  WiFi.setSleep(false);
  Serial.println("\n=== DASHBOARD F1 25 | MARCHA CENTRAL ===");
  Serial.println("UDP 20777, formato 2025. Comandos: i = informacoes; r = reconectar.");
  Serial.println("ERS: perfil F1 2025, bateria 4 MJ e saldo estimado por volta 4 MJ.");
  Serial.println("LDR digital: DO no GPIO34; brilho automatico no GPIO32.");
  nuvem.iniciar();
}

void loop() {
  const uint32_t agora = millis();
  atualizarLuminosidade(agora);
  nuvem.registrarAmostra(sensorLuz.valido(), sensorLuz.escuro(), brilhoAtual, agora);
  atualizarConexao(agora);
  receberTelemetria(agora);
  atualizarTela(agora);
  while (Serial.available() > 0) {
    const char comando = static_cast<char>(Serial.read());
    if (comando == 'i' || comando == 'I') relatarDados(agora);
    else if (comando == 'r' || comando == 'R') {
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
