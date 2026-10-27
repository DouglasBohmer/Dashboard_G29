#pragma once

#include <stdint.h>

namespace Configuracao {
constexpr uint8_t PINO_CS = 21;
constexpr uint8_t PINO_DC = 22;
constexpr uint8_t PINO_RESET = 4;
constexpr uint8_t PINO_MOSI = 23;
constexpr uint8_t PINO_MISO = 19;
constexpr uint8_t PINO_SCK = 18;
constexpr uint8_t PINO_BACKLIGHT = 32;
constexpr uint32_t FREQUENCIA_SPI = 10000000;
}  // namespace Configuracao
