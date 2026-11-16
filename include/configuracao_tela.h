#pragma once

#include <stdint.h>

namespace Configuracao {
constexpr int8_t PINO_CS = -1;
constexpr uint8_t PINO_DC = 22;
constexpr int8_t PINO_RESET = -1;
constexpr uint8_t PINO_MOSI = 23;
constexpr uint8_t PINO_MISO = 35;
constexpr uint8_t PINO_SCK = 18;
constexpr uint8_t PINO_BACKLIGHT = 32;
constexpr uint32_t FREQUENCIA_SPI = 10000000;
// Comandos de inicializacao e diagnostico em velocidade conservadora.
constexpr uint32_t FREQUENCIA_SPI_INICIALIZACAO = 1000000;
// Dar tempo para a alimentacao e o controlador do LCD estabilizarem ao ligar.
constexpr uint32_t ESPERA_INICIAL_TFT_MS = 500;
}  // namespace Configuracao
