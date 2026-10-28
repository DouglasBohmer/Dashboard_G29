#include "integracao_adafruit.h"

#include <WiFi.h>
#include <cstring>
#include <time.h>
#include "certificado_adafruit.h"

#if __has_include("configuracao_adafruit.local.h")
#include "configuracao_adafruit.local.h"
#else
#include "configuracao_adafruit.exemplo.h"
#endif

namespace AdafruitIO {
const char *Integracao::nomeEstado(Estado valor) {
  switch (valor) {
    case Estado::SemConfiguracao: return "sem configuracao local";
    case Estado::AguardandoWiFi: return "aguardando Wi-Fi";
    case Estado::AguardandoHorario: return "aguardando horario NTP";
    case Estado::Conectando: return "conectando por MQTT/TLS";
    case Estado::Conectado: return "conectado";
    default: return "falha; nova tentativa automatica";
  }
}

void Integracao::mudarEstado(Estado valor) {
  if (estado.exchange(valor) != valor) Serial.printf("Adafruit IO: %s.\n", nomeEstado(valor));
}

void Integracao::iniciar() {
  if (fila) return;
  if (!ConfiguracaoAdafruit::USUARIO[0] || !ConfiguracaoAdafruit::CHAVE[0] ||
      !ConfiguracaoAdafruit::FEED_LDR[0] || !ConfiguracaoAdafruit::FEED_BRILHO[0]) {
    Serial.println("Adafruit IO: preencha configuracao_adafruit.local.h para habilitar.");
    return;
  }
  const int tamanhoLdr = snprintf(topicoLdr, sizeof(topicoLdr), "%s/feeds/%s",
                                  ConfiguracaoAdafruit::USUARIO, ConfiguracaoAdafruit::FEED_LDR);
  const int tamanhoBrilho = snprintf(topicoBrilho, sizeof(topicoBrilho), "%s/feeds/%s",
                                     ConfiguracaoAdafruit::USUARIO, ConfiguracaoAdafruit::FEED_BRILHO);
  const int tamanhoErros = snprintf(topicoErros, sizeof(topicoErros), "%s/errors", ConfiguracaoAdafruit::USUARIO);
  const int tamanhoLimite = snprintf(topicoLimite, sizeof(topicoLimite), "%s/throttle", ConfiguracaoAdafruit::USUARIO);
  if (tamanhoLdr <= 0 || tamanhoLdr >= sizeof(topicoLdr) ||
      tamanhoBrilho <= 0 || tamanhoBrilho >= sizeof(topicoBrilho) ||
      tamanhoErros <= 0 || tamanhoErros >= sizeof(topicoErros) ||
      tamanhoLimite <= 0 || tamanhoLimite >= sizeof(topicoLimite) || !mqtt.setBufferSize(512)) {
    mudarEstado(Estado::Falha);
    Serial.println("Adafruit IO: topicos invalidos ou memoria insuficiente.");
    return;
  }
  const uint64_t mac = ESP.getEfuseMac();
  snprintf(identificador, sizeof(identificador), "dashboard-g29-%04X%08X",
           static_cast<unsigned>(mac >> 32), static_cast<unsigned>(mac));
  transporte.setCACert(CertificadoAdafruit::RAIZ);
  transporte.setTimeout(2);
  transporte.setHandshakeTimeout(5);
  mqtt.setServer("io.adafruit.com", 8883);
  mqtt.setSocketTimeout(2);
  mqtt.setKeepAlive(30);
  mqtt.setCallback([this](char *topico, uint8_t *mensagem, unsigned int tamanho) {
    receberAviso(topico, mensagem, tamanho);
  });
  fila = xQueueCreate(1, sizeof(Amostra));
  if (!fila) {
    mudarEstado(Estado::Falha);
    return;
  }
  mudarEstado(Estado::AguardandoWiFi);
  if (xTaskCreatePinnedToCore(executarTarefa, "adafruit_io", 12288, this, 1, nullptr, 0) != pdPASS) {
    vQueueDelete(fila);
    fila = nullptr;
    mudarEstado(Estado::Falha);
  }
}

void Integracao::registrarAmostra(bool valida, bool escuro, uint8_t brilho, uint32_t agora) {
  if (!fila || !coleta.consumir(agora, 100)) return;
  const Amostra amostra{valida, escuro, brilho, agora};
  xQueueOverwrite(fila, &amostra);
}

void Integracao::receberAviso(char *topico, uint8_t *, unsigned int) {
  ++falhas;
  if (std::strcmp(topico, topicoLimite) == 0) {
    pausadoPorLimite = true;
    inicioPausa = millis();
    Serial.println("Adafruit IO: limite de dados da conta atingido; pausa de 60 segundos.");
  } else Serial.println("Adafruit IO: aviso no canal de erros; consulte o Monitor da conta.");
}

void Integracao::executarTarefa(void *instancia) {
  static_cast<Integracao *>(instancia)->executar();
}

void Integracao::executar() {
  bool horarioSolicitado = false;
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(20));
    if (WiFi.status() != WL_CONNECTED) {
      transporte.stop();
      mudarEstado(Estado::AguardandoWiFi);
      continue;
    }
    if (!horarioSolicitado) {
      configTime(0, 0, "pool.ntp.org", "time.google.com");
      horarioSolicitado = true;
    }
    if (time(nullptr) < 1704067200) {
      mudarEstado(Estado::AguardandoHorario);
      continue;
    }
    if (!mqtt.connected()) {
      if (!reconexao.consumir(millis(), INTERVALO_RECONEXAO_MS)) continue;
      mudarEstado(Estado::Conectando);
      const bool conectado = mqtt.connect(identificador, ConfiguracaoAdafruit::USUARIO,
                                           ConfiguracaoAdafruit::CHAVE);
      codigoMqtt.store(mqtt.state());
      if (!conectado) {
        ++falhas;
        transporte.stop();
        mudarEstado(Estado::Falha);
        Serial.printf("Adafruit IO: codigo MQTT %ld.\n", static_cast<long>(codigoMqtt.load()));
        continue;
      }
      if (!mqtt.subscribe(topicoErros) || !mqtt.subscribe(topicoLimite)) {
        ++falhas;
        transporte.stop();
        mudarEstado(Estado::Falha);
        continue;
      }
      mudarEstado(Estado::Conectado);
    }
    if (!mqtt.loop()) {
      codigoMqtt.store(mqtt.state());
      mudarEstado(Estado::Falha);
      continue;
    }
    const uint32_t agora = millis();
    if (pausadoPorLimite && agora - inicioPausa < PAUSA_LIMITE_MS) continue;
    pausadoPorLimite = false;
    Amostra amostra{};
    if (xQueuePeek(fila, &amostra, 0) != pdTRUE || !amostra.valida ||
        agora - amostra.instante > VALIDADE_AMOSTRA_MS || !envio.consumir(agora, INTERVALO_ENVIO_MS)) continue;

    char brilho[4];
    snprintf(brilho, sizeof(brilho), "%u", percentualBrilho(amostra.brilho));
    // O feed LDR representa claro=0 e escuro=1, nao uma leitura analogica.
    const bool ldrEnviado = mqtt.publish(topicoLdr, amostra.escuro ? "1" : "0");
    const bool brilhoEnviado = mqtt.publish(topicoBrilho, brilho);
    pontosEnviados.fetch_add(static_cast<uint32_t>(ldrEnviado) + static_cast<uint32_t>(brilhoEnviado));
    if (!ldrEnviado || !brilhoEnviado) {
      ++falhas;
      codigoMqtt.store(mqtt.state());
      transporte.stop();
      mudarEstado(Estado::Falha);
    } else {
      Serial.printf("Adafruit IO: enviado claro/escuro=%u | brilho=%s%%.\n", amostra.escuro, brilho);
    }
  }
}

void Integracao::relatar() const {
  Serial.printf("Adafruit IO: %s | pontos enviados=%lu | falhas=%lu | codigo MQTT=%ld\n",
                nomeEstado(estado.load()), static_cast<unsigned long>(pontosEnviados.load()),
                static_cast<unsigned long>(falhas.load()), static_cast<long>(codigoMqtt.load()));
}
}  // namespace AdafruitIO
