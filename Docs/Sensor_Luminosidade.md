# Sensor de luminosidade digital

O modulo instalado possui tres pinos: VCC, GND e DO. Ele compara a luz com
um limite ajustado no potenciometro azul e entrega apenas dois estados.
Esta entrada nao mede percentual de iluminacao nem lux.

## Montagem

Com o USB desconectado, ligue:

| Modulo | ESP32 |
| --- | --- |
| VCC | 3V3, compartilhado com o VCC da TFT |
| GND | GND |
| DO | D34 / GPIO34 |

A protoboard distribui o 3V3 no ensaio atual. O LED da TFT continua no GPIO32,
com o mesmo resistor de 220 ou 300 ohms em serie. O modulo ja possui seu
circuito de comparacao; o divisor com resistor de 10 kohms pertence a outra
montagem, com LDR avulso.

## Comportamento

`include/sensor_luminosidade.h` concentra a configuracao e a logica:

- GPIO34 como entrada digital, lida a cada 20 ms.
- Um novo estado precisa permanecer estavel por 300 ms para ser aceito.
- Claro: PWM 255/255, aproximadamente 100% do ciclo ativo.
- Escuro: PWM 90/255, aproximadamente 35% do ciclo ativo.
- A transicao muda cinco unidades de PWM a cada 20 ms, sem espera bloqueante.
- PWM de 5 kHz, resolucao de 8 bits, canal 0, no GPIO32.
- Antes da primeira leitura confirmada, a tela permanece no brilho maximo.
- Sensor e controle de brilho funcionam mesmo sem Wi-Fi ou telemetria do F1.

Os percentuais representam o comando PWM, nao uma medicao de luz ou uma
garantia de brilho visual proporcional. Na tela conectada, o rodape inclui
`LUZ:CLA` ou `LUZ:ESC` e `B100%` ou `B35%`. Durante a transicao, o percentual
acompanha o PWM aplicado. No monitor serial, o relatorio a cada dois segundos
mostra DO, ambiente, brilho e disponibilidade do PWM; `i` repete o relatorio.

O codigo inicialmente interpreta DO alto como escuro. Confirme no modulo:
se a tela aumentar de brilho ao cobrir o LDR, altere
`NIVEL_ALTO_INDICA_ESCURO` de `true` para `false` e grave novamente. O estado
de um unico LED aceso no modulo nao confirma a leitura do ESP32.

## Validacao na bancada

1. Compile e grave o ambiente `telemetria_f1`.
2. Abra o monitor serial em 115200 baud.
3. Ilumine e cubra o elemento sensor, mantendo cada condicao por dois segundos.
4. Confira a mudanca de DO e de claro/escuro no relatorio e o brilho da TFT.
5. Se DO nao mudar, ajuste o potenciometro azul aos poucos ate obter a comutacao.
6. Posicione o sensor para receber a luz do ambiente, evitando a iluminacao
   direta da propria TFT, que pode causar alternancia entre os estados.

O projeto usa Arduino-ESP32 2.x e suas funcoes `ledcSetup`, `ledcAttachPin`
e `ledcWrite`; a API 3.x tem outra assinatura. Veja a
[documentacao da Espressif](https://docs.espressif.com/projects/arduino-esp32/en/latest/migration_guides/2.x_to_3.0.html#ledc).

`scripts/testar_luminosidade.ps1` executa 22 verificacoes em tempo de
compilacao: inicializacao, confirmacao, oscilacoes, virada do contador e
transicao de brilho. Ele nao grava nem valida a montagem fisica.

Em 28/10/2026, o ambiente `telemetria_f1` compilou com sucesso, as 22
verificacoes de luminosidade e as 26 do parser passaram. Nesta alteracao
foi feito somente o codigo; a gravacao e o teste do sensor permanecem pendentes.

## Adafruit IO

A integracao com Adafruit IO foi adiada a pedido do usuario. Esta versao
funciona localmente e nao publica dados na nuvem. Futuramente o feed deste
modulo representara claro/escuro; para luminosidade continua sera necessario
usar uma entrada analogica ou outro sensor.
