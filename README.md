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
| TFT SPI de 3,2 polegadas | KMRTM32032-SPI V2.0, sem touch. Ensaio manual confirmou cores, rotação, imagem 320×240 e ID compatível com ILI9341 (`41 00 93 41`). Tipo de painel e circuito interno do backlight ainda não confirmados. |
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
A ligação ESP32–TFT já foi montada e validada manualmente com o diagnóstico.
Os demais periféricos ainda serão integrados.

A pinagem apresentada no contexto inicial é provisória. Ela precisa ser
revisada considerando o novo display e os pinos de inicialização do ESP32.
A ligação definitiva do pino LED da TFT ao GPIO32 depende da confirmação
do circuito de acionamento existente na placa. No diagnóstico, usa-se um
resistor de 220 Ω ou 300 Ω em série para limitar a corrente nesse caminho.

## Confirmações pendentes de hardware

1. Verificar o circuito do pino LED da TFT para distinguir entrada de
   controle de conexão direta à iluminação. A inspeção e as medições de
   continuidade devem ocorrer com a placa desligada.
2. Identificar o código do display de sete segmentos e confirmar se é de
   ânodo comum ou cátodo comum, além da correspondência entre pinos e segmentos.
3. Medir os resistores e conferir as quantidades restantes; a leitura por
   foto de uma unidade indica 220 Ω e a caixa do kit lista unidades de 300 Ω.
4. Validar alimentação e pinagem antes da montagem definitiva.

## Estado do software

Há dois ambientes PlatformIO, configurados para ESP32 e porta COM3:

- `telemetria_f1` (padrão): `src/main.cpp` conecta às redes Wi-Fi cadastradas,
  recebe UDP F1 25 na porta 20777 e mostra velocidade, RPM e estado da conexão.
  O parser em `include/telemetria_f1.h` verifica formato, versão, tamanho,
  índice do jogador e valores; o receptor ignora quadros duplicados/atrasados
  e deixa de exibir valores antigos após 3 segundos sem atualização.
  Os números usam buffers de desenho na RAM para evitar o apagamento visível,
  com intervalo alvo de 33 ms; informações de diagnóstico usam 500 ms.
- `diagnostico_tft`: `src/diagnostico_tft.cpp` executa o teste de cores,
  orientação, texto e leitura do controlador, já validado manualmente.

O mesmo receptor atende PS5 por cabo ou por Wi-Fi na rede local. O ESP32
sempre usa Wi-Fi de 2,4 GHz. Casa e faculdade podem ser cadastradas em
`include/configuracao_wifi.local.h`, ignorado pelo Git; o exemplo público é
`include/configuracao_wifi.exemplo.h`. Sem credenciais, a tela solicita a
configuração em vez de tentar uma rede fictícia. Redes institucionais com
portal, autenticação empresarial ou isolamento entre clientes exigem
avaliação específica.

Redes com senha exigem WPA2 por padrão. Para um roteador que use apenas
WPA/TKIP, a opção `permitirWpaLegado` habilita compatibilidade somente na
rede cadastrada; o monitor serial também registra os motivos de desconexão.
O ensaio de bancada confirmou conexão Wi-Fi e obtenção de IP por DHCP após
esse ajuste. O usuário confirmou funcionamento com o F1 25 no PS5, e o
monitor serial confirmou recepção de telemetria no formato 2025.

Veja [Teste da TFT](Docs/Teste_TFT.md) para as ligações e
[Teste F1 25](Docs/Teste_F1_25.md) para configurar as redes e o jogo.
`scripts/testar_parser.ps1` executa 26 verificações em tempo de compilação
sem gravar no ESP32. O teste na rede da faculdade ainda está pendente.

O backend, o LDR, os LEDs de RPM e o indicador de marchas ainda não foram
integrados. O GPIO32 permanece em nível alto, com resistor em série no ramo
do backlight e sem PWM. A marcha é registrada apenas no monitor serial
do teste UDP; sua exibição ficará no indicador dedicado.

O código do projeto deverá usar nomes e comentários em português do Brasil.
As orientações da disciplina e os prints das compras estão em `Docs/`.

## Referências técnicas

- [Especificação UDP do F1 25 — EA](https://forums.ea.com/t5/s/tghpe58374/attachments/tghpe58374/f1-games-game-info-hub-en/61/4/Data%20Output%20from%20F1%2025%20v3.pdf)
- [Datasheet do ILI9341 — fabricante, hospedado pela Adafruit](https://cdn-shop.adafruit.com/datasheets/ILI9341.pdf)
- [Cálculo de resistores para LEDs — Adafruit](https://learn.adafruit.com/all-about-leds/forward-voltage-and-kvl)

A compatibilidade com ILI9341 foi verificada no ensaio manual; isso não
identifica, por si só, o tipo de painel ou o circuito interno do backlight.
