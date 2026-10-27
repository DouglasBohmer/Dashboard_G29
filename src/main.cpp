#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

namespace Configuracao {
constexpr uint8_t PINO_CS = 21;
constexpr uint8_t PINO_DC = 22;
constexpr uint8_t PINO_RESET = 4;
constexpr uint8_t PINO_MOSI = 23;
constexpr uint8_t PINO_MISO = 19;
constexpr uint8_t PINO_SCK = 18;
constexpr uint8_t PINO_BACKLIGHT = 32;
constexpr uint32_t FREQUENCIA_SPI = 10000000;
constexpr uint8_t COMANDO_IDENTIFICACAO = 0xD3;
}  // namespace Configuracao

Adafruit_ILI9341 tela(Configuracao::PINO_CS, Configuracao::PINO_DC,
                      Configuracao::PINO_RESET);

uint8_t identificacao[4] = {};
bool identificacaoCompativel = false;
uint8_t rotacaoAtual = 1;
uint32_t ultimaAtualizacao = 0;

void lerIdentificacao() {
  Serial.println("\nLeitura do controlador pelo comando 0xD3:");

  for (uint8_t indice = 0; indice < 4; ++indice) {
    identificacao[indice] =
        tela.readcommand8(Configuracao::COMANDO_IDENTIFICACAO, indice);
    Serial.printf("  Byte %u: 0x%02X\n", indice, identificacao[indice]);
  }

  identificacaoCompativel = false;
  for (uint8_t indice = 0; indice < 3; ++indice) {
    if (identificacao[indice] == 0x93 && identificacao[indice + 1] == 0x41) {
      identificacaoCompativel = true;
    }
  }

  if (identificacaoCompativel) {
    Serial.println("Identificacao compativel com ILI9341 (0x9341).");
  } else {
    Serial.println("Identificacao inconclusiva para ILI9341.");
    Serial.println("Zeros ou FF podem indicar falha de leitura ou de ligacao.");
    Serial.println("A leitura inconclusiva, sozinha, nao comprova defeito.");
  }
}

void desenharOrientacao(uint8_t rotacao) {
  tela.setRotation(rotacao);
  tela.fillScreen(ILI9341_BLACK);
  tela.drawRect(0, 0, tela.width(), tela.height(), ILI9341_WHITE);
  tela.drawLine(0, 0, tela.width() - 1, tela.height() - 1, ILI9341_RED);
  tela.drawLine(tela.width() - 1, 0, 0, tela.height() - 1, ILI9341_GREEN);
  tela.setTextSize(2);
  tela.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
  tela.setCursor(8, 8);
  tela.println("TOPO ESQUERDO");
  tela.setCursor(8, 36);
  tela.printf("Rotacao: %u", rotacao);
  tela.setCursor(8, 64);
  tela.printf("%d x %d", tela.width(), tela.height());
  tela.setCursor(8, tela.height() - 24);
  tela.print("BASE ESQUERDA");
}

void desenharPainel() {
  tela.setRotation(rotacaoAtual);
  tela.fillScreen(ILI9341_BLACK);
  tela.drawRect(0, 0, tela.width(), tela.height(), ILI9341_WHITE);
  tela.setTextWrap(false);
  tela.setTextSize(2);
  tela.setTextColor(ILI9341_CYAN);
  tela.setCursor(10, 12);
  tela.print("DIAGNOSTICO TFT");
  tela.setTextColor(ILI9341_WHITE);
  tela.setCursor(10, 44);
  tela.printf("%d x %d pixels", tela.width(), tela.height());
  tela.setCursor(10, 72);
  tela.printf("ID: %02X %02X %02X %02X", identificacao[0], identificacao[1],
              identificacao[2], identificacao[3]);
  tela.setCursor(10, 100);
  tela.setTextColor(identificacaoCompativel ? ILI9341_GREEN : ILI9341_YELLOW);
  tela.print(identificacaoCompativel ? "ID compativel: 9341" : "ID inconclusivo");
  tela.setTextColor(ILI9341_WHITE);
  tela.setCursor(10, 134);
  tela.print("Tempo ligado:");
  tela.setTextSize(1);
  tela.setCursor(10, tela.height() - 32);
  tela.print("Serial: t=testes i=ID r=rotacao");
  tela.setCursor(10, tela.height() - 18);
  tela.print("LED: GPIO32 + resistor em serie.");
  ultimaAtualizacao = millis() - 1000;
}

void executarTestesVisuais() {
  const uint16_t cores[] = {ILI9341_RED, ILI9341_GREEN, ILI9341_BLUE,
                            ILI9341_WHITE, ILI9341_BLACK};
  const char *nomes[] = {"VERMELHO", "VERDE", "AZUL", "BRANCO", "PRETO"};

  tela.setRotation(1);
  for (uint8_t indice = 0; indice < 5; ++indice) {
    Serial.printf("Teste de cor: %s\n", nomes[indice]);
    tela.fillScreen(cores[indice]);
    delay(1500);
  }

  for (uint8_t rotacao = 0; rotacao < 4; ++rotacao) {
    Serial.printf("Teste de orientacao: rotacao %u\n", rotacao);
    desenharOrientacao(rotacao);
    delay(1500);
  }

  desenharPainel();
  Serial.println("Sequencia enviada. Confira cores, bordas e texto na tela.");
  Serial.println("Comandos: t = repetir testes; i = reler ID; r = girar tela.");
}

void setup() {
  // O pino LED da TFT exige resistor de 220 ou 300 ohms neste ensaio.
  pinMode(Configuracao::PINO_BACKLIGHT, OUTPUT);
  digitalWrite(Configuracao::PINO_BACKLIGHT, HIGH);
  Serial.begin(115200);
  delay(1200);
  Serial.println("\n=== DIAGNOSTICO TFT - ESP32 ===");
  Serial.println("CS=21 DC=22 RESET=4 MOSI=23 MISO=19 SCK=18");
  Serial.println("LED=32 com resistor de 220 ou 300 ohms em serie; sem PWM.");
  Serial.println("Driver ILI9341 em teste; confirmacao depende do resultado.");

  SPI.begin(Configuracao::PINO_SCK, Configuracao::PINO_MISO,
            Configuracao::PINO_MOSI, Configuracao::PINO_CS);
  tela.begin(Configuracao::FREQUENCIA_SPI);
  lerIdentificacao();
  executarTestesVisuais();
}

void loop() {
  while (Serial.available() > 0) {
    const char comando = static_cast<char>(Serial.read());
    if (comando == 't' || comando == 'T') {
      executarTestesVisuais();
    } else if (comando == 'i' || comando == 'I') {
      lerIdentificacao();
      desenharPainel();
    } else if (comando == 'r' || comando == 'R') {
      rotacaoAtual = (rotacaoAtual + 1) % 4;
      desenharOrientacao(rotacaoAtual);
      delay(1500);
      desenharPainel();
    }
  }

  const uint32_t agora = millis();
  if (agora - ultimaAtualizacao >= 1000) {
    ultimaAtualizacao = agora;
    tela.fillRect(10, 158, tela.width() - 20, 22, ILI9341_BLACK);
    tela.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
    tela.setTextSize(2);
    tela.setCursor(10, 158);
    tela.printf("%lu s", static_cast<unsigned long>(agora / 1000));
  }
  delay(10);
}
