#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Adafruit_ILI9341.h>
#include <DHTesp.h>
#include "configuracao_tela.h"
#include "dashboard_f1.h"
#include "telas/principal_f1_2025.h"
#include "telas/temperaturas_f1_2025.h"
#include "sensor_luminosidade.h"
#include "integracao_adafruit.h"
#include "indicadores_painel.h"
#include "rede_painel.h"
#include "demonstracao_dashboard.h"

#if __has_include("configuracao_wifi.local.h")
#include "configuracao_wifi.local.h"
#else
#include "configuracao_wifi.exemplo.h"
#endif

namespace Configuracao {
constexpr uint16_t PORTA_UDP = RedePainel::PORTA_UDP;
constexpr uint32_t TEMPO_TENTATIVA_WIFI = 15000;
constexpr uint32_t INTERVALO_TELA = 33;
constexpr size_t TAMANHO_BUFFER = 1536;
constexpr uint8_t PINO_DHT22 = 33;
constexpr uint32_t INTERVALO_DHT22_MS = 2500;
// Use false para voltar aos LEDs comandados pela telemetria do F1.
constexpr bool TESTE_LEDS_SEQUENCIAL = false;
constexpr uint32_t INTERVALO_TESTE_LEDS_MS = 300;
}

Adafruit_ILI9341 tela(Configuracao::PINO_CS, Configuracao::PINO_DC,
                      Configuracao::PINO_RESET);
InterfaceDashboard::CamposBufferizados camposTela(tela);
InterfaceDashboard::PrincipalF12025 painel(tela, camposTela);
InterfaceDashboard::TemperaturasF12025 painelTemperaturas(tela, camposTela);
PerfisDashboard::Selecao selecaoTela;
PerfisDashboard::Selecao selecaoAntesDemonstracao;
DemonstracaoDashboard::Demonstracao demonstracao;
DemonstracaoDashboard::Etapa ultimaEtapaDemo = DemonstracaoDashboard::Etapa::Acelerando;
bool redesenharPagina = false;
DashboardF1::Telemetria dados;
Luminosidade::Sensor sensorLuz;
AdafruitIO::Integracao nuvem;
RedePainel::Gerenciador redePainel;
DHTesp sensorAmbiente;
TempAndHumidity ambiente{NAN, NAN};
bool tentouLerAmbiente = false, ambienteValido = false;
uint32_t ultimaLeituraAmbiente = 0;
Indicadores::Estado estadoIndicadores{};
uint32_t inicioTesteLeds = 0;
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

void iniciarIndicadores() {
  for (uint8_t pino : Indicadores::PINOS) {
    digitalWrite(pino, LOW);
    pinMode(pino, OUTPUT);
  }
}

size_t etapaTesteLeds(uint32_t agora) {
  return ((agora - inicioTesteLeds) / Configuracao::INTERVALO_TESTE_LEDS_MS) %
         Indicadores::QUANTIDADE_CANAIS;
}

void atualizarIndicadores(uint32_t agora) {
  Indicadores::Estado novoEstado{};
  if (Configuracao::TESTE_LEDS_SEQUENCIAL) {
    novoEstado[etapaTesteLeds(agora)] = true;
  } else {
    const auto &telemetria = demonstracao.ativa() ? demonstracao.dados() : dados;
    const bool permitirDados = demonstracao.ativa() || (WiFi.status() == WL_CONNECTED && udpAtivo);
    if (permitirDados && !telemetria.pausado(agora) && telemetria.sessaoRecente(agora) &&
        PerfisDashboard::compativel(selecaoTela.atual(), F1::FORMATO, telemetria.formula)) {
      const bool carroValido = telemetria.carroRecente(agora);
      const bool estadoValido = telemetria.energiaRecente(agora);
      novoEstado = Indicadores::calcularEstado(
          carroValido ? telemetria.carro.luzesRpm : 0,
          estadoValido && telemetria.energia.drsPermitido,
          carroValido && telemetria.carro.drsAtivo,
          estadoValido ? telemetria.bandeiraFia : -1, agora);
    }
  }
  for (size_t indice = 0; indice < Indicadores::PINOS.size(); ++indice) {
    if (novoEstado[indice] == estadoIndicadores[indice]) continue;
    digitalWrite(Indicadores::PINOS[indice], novoEstado[indice] ? HIGH : LOW);
    if (Configuracao::TESTE_LEDS_SEQUENCIAL && novoEstado[indice]) {
      constexpr const char *nomes[] = {
          "RPM 01 (esquerda)", "RPM 02", "RPM 03", "RPM 04", "RPM 05",
          "RPM 06", "RPM 07", "RPM 08", "RPM 09", "RPM 10 (direita)",
          "11 branco esquerdo", "12 branco direito", "13+14 amarelos",
          "15+16 verdes"};
      Serial.printf("Teste LEDs: %s | GPIO%u | aceso por %lu ms\n",
                    nomes[indice], Indicadores::PINOS[indice],
                    static_cast<unsigned long>(Configuracao::INTERVALO_TESTE_LEDS_MS));
    }
  }
  estadoIndicadores = novoEstado;
}

void atualizarAmbiente(uint32_t agora) {
  if (agora - ultimaLeituraAmbiente < Configuracao::INTERVALO_DHT22_MS) return;
  ultimaLeituraAmbiente = agora;
  ambiente = sensorAmbiente.getTempAndHumidity();
  tentouLerAmbiente = true;
  ambienteValido = sensorAmbiente.getStatus() == DHTesp::ERROR_NONE &&
                   std::isfinite(ambiente.temperature) && std::isfinite(ambiente.humidity) &&
                   ambiente.temperature >= -40 && ambiente.temperature <= 80 &&
                   ambiente.humidity >= 0 && ambiente.humidity <= 100;
}

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
    const bool nivel = digitalRead(ConfiguracaoLuminosidade::PINO_SENSOR) == HIGH;
    const bool eraValido = sensorLuz.valido(), eraEscuro = sensorLuz.escuro();
    sensorLuz.atualizar(nivel, agora);
    if (sensorLuz.valido() && (!eraValido || eraEscuro != sensorLuz.escuro())) {
      Serial.printf("LDR confirmado: DO=%u | %s | alvo=%u%%\n", nivel,
                    sensorLuz.escuro() ? "escuro" : "claro",
                    (sensorLuz.brilhoAlvo() * 100U + 127U) / 255U);
    }
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
    if (redePainel.pronto() && redePainel.destino() != WiFi.localIP()) redePainel.reiniciar();
    redePainel.atualizar(agora);
    if (!redePainel.pronto()) return;
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
  redePainel.reiniciar();
  if (tentandoWiFi && agora - inicioTentativa < Configuracao::TEMPO_TENTATIVA_WIFI) return;
  WiFi.disconnect();
  for (size_t tentativa = 0; tentativa < ConfiguracaoWiFi::QUANTIDADE_REDES; ++tentativa) {
    const auto &rede = ConfiguracaoWiFi::REDES[proximaRede];
    proximaRede = (proximaRede + 1) % ConfiguracaoWiFi::QUANTIDADE_REDES;
    if (rede.nome[0] == '\0') continue;
    nomeRedeEmTeste = rede.nome;
    inicioTentativa = agora;
    tentandoWiFi = true;
    // Descobrir a sub-rede/gateway por DHCP antes de escolher um dos cinco IPs.
    if (!WiFi.config(IPAddress(), IPAddress(), IPAddress())) {
      Serial.println("Rede: falha ao iniciar a descoberta por DHCP.");
      tentandoWiFi = false;
      return;
    }
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
    if (demonstracao.registrarTelemetriaReal(agora)) {
      selecaoTela = selecaoAntesDemonstracao;
      redesenharPagina = true;
      Serial.println("DEMO encerrada: telemetria real recebida. Retomando a pagina anterior.");
    }
    if (dados.consumirAcaoUdp1()) {
      selecaoTela.proxima();
      redesenharPagina = true;
      Serial.printf("Acao UDP 1: tela %s.\n", selecaoTela.atual().nome);
    }
  }
}

void atualizarDemonstracao(uint32_t agora) {
  if (demonstracao.atualizar(agora)) {
    selecaoAntesDemonstracao = selecaoTela;
    selecaoTela = PerfisDashboard::Selecao{};
    ultimaEtapaDemo = demonstracao.etapa();
    redesenharPagina = true;
    Serial.println("DEMO: 10 s sem telemetria valida. Wi-Fi e UDP continuam ativos em segundo plano.");
    Serial.printf("DEMO: %s.\n", DemonstracaoDashboard::nomeEtapa(ultimaEtapaDemo));
  }
  if (!demonstracao.ativa()) return;
  if (demonstracao.etapa() != ultimaEtapaDemo) {
    ultimaEtapaDemo = demonstracao.etapa();
    Serial.printf("DEMO: %s.\n", DemonstracaoDashboard::nomeEtapa(ultimaEtapaDemo));
  }
  if (selecaoTela.atual().pagina != demonstracao.pagina()) {
    selecaoTela.proxima();
    redesenharPagina = true;
  }
}

void atualizarTela(uint32_t agora) {
  if (agora - ultimaTela < Configuracao::INTERVALO_TELA) return;
  ultimaTela = agora;
  const uint32_t inicio = micros();
  const bool conectado = WiFi.status() == WL_CONNECTED;
  const auto &telemetria = demonstracao.ativa() ? demonstracao.dados() : dados;
  const bool perfilCompativel = telemetria.sessaoRecente(agora) &&
      PerfisDashboard::compativel(selecaoTela.atual(), F1::FORMATO, telemetria.formula);
  InterfaceDashboard::InformacoesConexao conexao;
  conexao.jogoConectado = conectado && udpAtivo && dados.carroRecente(agora);
  if (demonstracao.ativa()) conexao.etapaDemo = DemonstracaoDashboard::nomeEtapa(demonstracao.etapa());
  conexao.wifi = conectado ? String("WiFi: ") + InterfaceDashboard::nomeExibivel(WiFi.SSID().c_str()) :
      !haRedesConfiguradas ? String("WiFi: configure configuracao_wifi.local.h") :
      String("WiFi: tentando ") + InterfaceDashboard::nomeExibivel(nomeRedeEmTeste);
  if (conectado && redePainel.pronto()) {
    const auto ip = redePainel.destino();
    conexao.destino = InterfaceDashboard::formatarDestinoF1(
        {ip[0], ip[1], ip[2], ip[3]}, redePainel.opcaoAtual()) + " | UDP 2025";
  } else conexao.destino = conectado ? String("WiFi conectado | verificando IP do painel") :
                                      String("ESP32 e jogo na mesma rede | WiFi 2.4 GHz");
  const bool permitirDados = perfilCompativel && (demonstracao.ativa() || (conectado && udpAtivo));
  const bool temperaturas = selecaoTela.atual().pagina == PerfisDashboard::Pagina::Temperaturas;
  if (redesenharPagina) {
    if (temperaturas) painelTemperaturas.iniciar();
    else painel.iniciar();
    redesenharPagina = false;
  }
  if (temperaturas) painelTemperaturas.atualizar(telemetria, agora, permitirDados, &conexao);
  else painel.atualizar(telemetria, agora, permitirDados, &conexao);
  const uint32_t tempo = micros() - inicio;
  if (tempo > maiorTempoDesenhoUs) maiorTempoDesenhoUs = tempo;
}

void relatarDados(uint32_t agora) {
  Serial.printf("Fonte: %s%s | Wi-Fi: %s | destino F1: %s:%u | F1 25 > Configuracoes > Telemetria > formato 2025\n",
                demonstracao.ativa() ? "DEMO / " : "",
                demonstracao.ativa() ? DemonstracaoDashboard::nomeEtapa(demonstracao.etapa()) : "REAL / AGUARDANDO JOGO",
                WiFi.status() == WL_CONNECTED ? WiFi.SSID().c_str() : "desconectado",
                redePainel.pronto() ? redePainel.destino().toString().c_str() : "aguardando IP",
                Configuracao::PORTA_UDP);
  Serial.printf("WiFi=%s | IP=%s | UDP=%lu | atualizados=%lu | descartados=%lu | atrasados=%lu | formato=%u\n",
                WiFi.status() == WL_CONNECTED ? "conectado" : "desconectado",
                WiFi.localIP().toString().c_str(), static_cast<unsigned long>(pacotesRecebidos),
                static_cast<unsigned long>(pacotesAtualizados), static_cast<unsigned long>(pacotesDescartados),
                static_cast<unsigned long>(pacotesForaDeOrdem), formatoRecebido);
  Serial.printf("Pacotes: sessao=%lu | voltas=%lu | participantes=%lu | carro=%lu | energia=%lu | historico=%lu\n",
                static_cast<unsigned long>(pacotesPorTipo[1]), static_cast<unsigned long>(pacotesPorTipo[2]),
                static_cast<unsigned long>(pacotesPorTipo[4]),
                static_cast<unsigned long>(pacotesPorTipo[6]), static_cast<unsigned long>(pacotesPorTipo[7]),
                static_cast<unsigned long>(pacotesPorTipo[11]));
  Serial.printf("Tela: alvo=%lu ms | maior desenho=%lu us | heap livre=%u\n",
                static_cast<unsigned long>(Configuracao::INTERVALO_TELA),
                static_cast<unsigned long>(maiorTempoDesenhoUs), ESP.getFreeHeap());
  maiorTempoDesenhoUs = 0;
  Serial.printf("Pagina: %s | Acao UDP 1 alterna as telas.\n", selecaoTela.atual().nome);
  Serial.printf("Luz: DO=%u | ambiente=%s | brilho PWM=%u%% | PWM=%s\n",
                digitalRead(ConfiguracaoLuminosidade::PINO_SENSOR),
                !sensorLuz.valido() ? "estabilizando" : sensorLuz.escuro() ? "escuro" : "claro",
                (brilhoAtual * 100U + 127U) / 255U, pwmDisponivel ? "ativo" : "indisponivel");
  if (!tentouLerAmbiente) {
    Serial.println("DHT22: aguardando a primeira leitura.");
  } else if (ambienteValido) {
    Serial.printf("DHT22: temperatura=%.1f C | umidade=%.1f%%\n",
                  ambiente.temperature, ambiente.humidity);
  } else {
    const char *falha = sensorAmbiente.getStatus() == DHTesp::ERROR_NONE
                            ? "valores fora da faixa" : sensorAmbiente.getStatusString();
    Serial.printf("DHT22: leitura invalida (%s). Confira GPIO33 e o pull-up externo de 10 kohms entre OUT e 3V3.\n",
                  falha);
  }
  Serial.printf("Indicadores: DRS disponivel=%u | aberto=%u | amarela=%u | verde=%u\n",
                estadoIndicadores[Indicadores::DRS_DISPONIVEL],
                estadoIndicadores[Indicadores::DRS_ABERTO],
                estadoIndicadores[Indicadores::BANDEIRA_AMARELA],
                estadoIndicadores[Indicadores::BANDEIRA_VERDE]);
  nuvem.relatar();
  if (dados.carroRecente(agora)) {
    Serial.printf("Origem=%s | velocidade=%u | RPM=%u | marcha=%d | DRS=%u | luzes=%u%%\n",
                  origemTelemetria.toString().c_str(), dados.carro.velocidadeKmh, dados.carro.rpm,
                  dados.carro.marcha, dados.carro.drsAtivo, dados.carro.luzesRpm);
    constexpr F1::Roda ordem[] = {F1::DianteiraEsquerda, F1::DianteiraDireita,
                                F1::TraseiraEsquerda, F1::TraseiraDireita};
    constexpr const char *nomes[] = {"DE", "DD", "TE", "TD"};
    for (uint8_t indice = 0; indice < 4; ++indice) {
      const auto roda = ordem[indice];
      Serial.printf("%s: pneu interno=%u C | externo=%u C | freio=%u C\n", nomes[indice],
                    dados.carro.temperaturaPneusInterna[roda], dados.carro.temperaturaPneusExterna[roda],
                    dados.carro.temperaturaFreios[roda]);
    }
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
  iniciarIndicadores();
  Serial.begin(115200);
  sensorAmbiente.setup(Configuracao::PINO_DHT22, DHTesp::DHT22);
  pinMode(Configuracao::PINO_DHT22, INPUT);
  ultimaLeituraAmbiente = millis();
  iniciarLuminosidade();
  if (!camposTela.memoriaDisponivel()) Serial.println("Falha ao reservar buffer de desenho da TFT.");
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
  Serial.printf("\n=== %s ===\n", selecaoTela.atual().nome);
  Serial.println("Acao UDP 1: alternar Principal / Pneus e freios. Um toque, uma troca.");
  Serial.println("UDP 20777, formato 2025. Comandos: i = informacoes; r = reconectar.");
  Serial.println("Rede: preferencia 192.168.8.80, alternativas .81 a .84, com verificacao ARP.");
  Serial.println("Em outra sub-rede, manter terminacoes .80 a .84 e usar o IP deste Monitor Serial.");
  Serial.println("ERS: perfil F1 2025, bateria 4 MJ e saldo estimado por volta 4 MJ.");
  Serial.println("LDR digital: DO no GPIO34; brilho automatico no GPIO32.");
  Serial.println("TFT: CS em GND, RESET em 3V3, MISO no GPIO35 e reset por software.");
  Serial.println("DHT22: OUT no GPIO33; leitura a cada 2,5 segundos.");
  Serial.println("LEDs: dez RPM independentes, dois DRS, amarelos no GPIO2 e verdes no GPIO12.");
  if (Configuracao::TESTE_LEDS_SEQUENCIAL) {
    Serial.printf("TESTE LEDs ativo: um canal por vez, %lu ms, repetindo a partir da esquerda.\n",
                  static_cast<unsigned long>(Configuracao::INTERVALO_TESTE_LEDS_MS));
    Serial.println("Ordem: RPM 01-10, branco esquerdo, branco direito, par amarelo, par verde.");
    Serial.println("Os 16 LEDs usam 14 canais; cada par lateral acende junto. O teste dispensa Wi-Fi e F1.");
  } else {
    Serial.println("RPM: segue as luzes de troca do jogo. Ordem: 2 verdes, 2 amarelos, 3 vermelhos, 3 azuis.");
    Serial.println("Bandeiras piscam a cada 500 ms; azul pisca os dois brancos com prioridade sobre DRS.");
    Serial.println("Sem bandeira azul, DRS disponivel permanece aceso com a asa aberta.");
  }
  nuvem.iniciar();
  inicioTesteLeds = millis();
  demonstracao.aguardar(inicioTesteLeds);
  Serial.println("DEMO automatica apos 10 s sem jogo; qualquer pacote F1 valido devolve prioridade aos dados reais.");
  atualizarIndicadores(inicioTesteLeds);
}

void loop() {
  const uint32_t agora = millis();
  atualizarLuminosidade(agora);
  nuvem.registrarAmostra(sensorLuz.valido(), sensorLuz.escuro(), brilhoAtual, agora);
  atualizarConexao(agora);
  receberTelemetria(agora);
  atualizarDemonstracao(agora);
  atualizarIndicadores(agora);
  atualizarAmbiente(agora);
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
