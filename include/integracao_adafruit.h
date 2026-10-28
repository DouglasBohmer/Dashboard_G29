#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "controle_adafruit.h"

namespace AdafruitIO {
class Integracao {
 public:
  void iniciar();
  void registrarAmostra(bool valida, bool escuro, uint8_t brilho, uint32_t agora);
  void relatar() const;

 private:
  enum class Estado : uint32_t {
    SemConfiguracao, AguardandoWiFi, AguardandoHorario, Conectando, Conectado, Falha
  };
  struct Amostra {
    bool valida, escuro;
    uint8_t brilho;
    uint32_t instante;
  };

  WiFiClientSecure transporte;
  PubSubClient mqtt{transporte};
  QueueHandle_t fila = nullptr;
  std::atomic<Estado> estado{Estado::SemConfiguracao};
  std::atomic<uint32_t> pontosEnviados{0}, falhas{0};
  std::atomic<int32_t> codigoMqtt{-1};
  char topicoLdr[128]{}, topicoBrilho[128]{};
  char topicoErros[128]{}, topicoLimite[128]{};
  char identificador[40]{};
  bool pausadoPorLimite = false;
  uint32_t inicioPausa = 0;
  Temporizador envio, reconexao, coleta;

  static void executarTarefa(void *instancia);
  static const char *nomeEstado(Estado valor);
  void executar();
  void mudarEstado(Estado valor);
  void receberAviso(char *topico, uint8_t *mensagem, unsigned int tamanho);
};
}  // namespace AdafruitIO
