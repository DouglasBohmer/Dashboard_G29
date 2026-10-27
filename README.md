# Dashboard IoT G29

Projeto de Automação IoT da disciplina de Internet das Coisas da Católica SC,
voltado à telemetria de Sim Racing com Logitech G29.

## Funcionamento previsto

O F1 25 no PS5 enviará telemetria UDP pela rede local ao ESP32. O dispositivo
exibirá dados na tela TFT, indicará as marchas em um display dedicado de sete
segmentos e acionará cinco LEDs conforme o RPM. Um sensor LDR ajustará
automaticamente o brilho da TFT. Estatísticas de voltas e sessões serão
enviadas a um backend Spring Boot para registro e consulta.

O formato UDP inicial previsto é 2025, a ser selecionado também no jogo.

## Componentes e funções

| Componente | Função e situação |
| --- | --- |
| ESP32 WROOM, 30 pinos, USB-C | Comunicação Wi-Fi, processamento e controle das saídas. |
| TFT SPI de 3,2 polegadas | Velocidade e demais informações da sessão. Placa identificada na foto como KMRTM32032-SPI, 240×320, V2.0. Anúncio informa ausência de touch. Controlador, tipo de painel e circuito do backlight pendentes de confirmação. |
| Display de sete segmentos, um dígito | Indicador exclusivo de marchas, proveniente do Kit Iniciante V8 da RoboCore. A referência do fabricante indica vermelho e cátodo comum; confirmar modelo e pinagem na peça antes de montar. |
| Cinco LEDs discretos | Barra de RPM: dois verdes, um amarelo e dois vermelhos. |
| Módulo LDR com saída analógica | Medição de luminosidade para ajuste do brilho da TFT. |
| Resistores | Foto de uma unidade compatível com 220 Ω ±5%. A caixa do kit também lista 15 resistores de 300 Ω e cinco de 10 kΩ. Quantidades restantes e valores medidos ainda não conferidos. |

## Indicador dedicado de marchas

- As marchas serão exibidas exclusivamente no display de sete segmentos.
- O comportamento previsto é mostrar `1` a `8`, `n` para neutro e `r` para ré,
  com letras aproximadas pelos segmentos disponíveis.
- O ponto decimal não é necessário para essa função.
- Se utilizado sem um circuito controlador, serão necessárias sete saídas
  digitais e um resistor em série para cada segmento.
- A sugestão inicial é 330 Ω por segmento para LEDs indicadores comuns em
  3,3 V; o valor final depende do componente e do brilho desejado.
- A escolha entre acionamento direto e circuito controlador, assim como a
  pinagem completa, será definida após identificar o display.

## Montagem

A montagem será feita sem protoboard, com ligações diretas por fios, soldas
ou conectores, GND comum, isolamento das conexões e fixação dos componentes.
Nenhuma ligação física havia sido feita na última atualização.

A pinagem apresentada no contexto inicial é provisória. Ela precisa ser
revisada considerando o novo display e os pinos de inicialização do ESP32.
A ligação definitiva do pino LED da TFT ao GPIO32 depende da confirmação
do circuito de acionamento existente na placa. No diagnóstico, usa-se um
resistor de 220 Ω ou 300 Ω em série para limitar a corrente nesse caminho.

## Confirmações pendentes de hardware

1. Identificar o controlador da TFT por documentação do módulo ou leitura de
   identificação via SPI. Para essa leitura, será necessário ligar também
   o SDO/MISO da TFT a uma entrada SPI do ESP32; GPIO19 é uma opção para o
   diagnóstico, ainda sem atribuição definitiva no projeto.
2. Verificar o circuito do pino LED da TFT para distinguir entrada de
   controle de conexão direta à iluminação. A inspeção e as medições de
   continuidade devem ocorrer com a placa desligada.
3. Identificar o código do display de sete segmentos e confirmar se é de
   ânodo comum ou cátodo comum, além da correspondência entre pinos e segmentos.
4. Medir os resistores e conferir as quantidades restantes; a leitura por
   foto de uma unidade indica 220 Ω e a caixa do kit lista unidades de 300 Ω.
5. Validar alimentação e pinagem antes da montagem definitiva.

## Estado do software

Existe um firmware inicial de diagnóstico da TFT em `src/main.cpp`, com
teste de cores, orientação, texto e tentativa de leitura do controlador.
O ambiente `diagnostico_tft` está configurado para ESP32 e porta COM3.
As ligações específicas e as instruções estão em [Teste da TFT](Docs/Teste_TFT.md).

O firmware de telemetria e o backend ainda não foram implementados. O teste
não aciona os demais periféricos. O GPIO32 permanece em nível alto, com
resistor em série no ramo do backlight e sem PWM. A validação física
da imagem e do controlador depende do ensaio com a tela conectada.

O código do projeto deverá usar nomes e comentários em português do Brasil.
As orientações da disciplina e os prints das compras estão em `Docs/`.

## Referências técnicas

- [Especificação UDP do F1 25 — EA](https://forums.ea.com/t5/s/tghpe58374/attachments/tghpe58374/f1-games-game-info-hub-en/61/4/Data%20Output%20from%20F1%2025%20v3.pdf)
- [Datasheet do ILI9341 — fabricante, hospedado pela Adafruit](https://cdn-shop.adafruit.com/datasheets/ILI9341.pdf)
- [Cálculo de resistores para LEDs — Adafruit](https://learn.adafruit.com/all-about-leds/forward-voltage-and-kvl)

A referência do ILI9341 servirá para comparação e diagnóstico; sua inclusão
não confirma que esse seja o controlador da TFT adquirida.
