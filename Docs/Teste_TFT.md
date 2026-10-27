# Teste da TFT com o ESP32

Este firmware verifica cores, orientação, texto e leitura do controlador via
SPI. O driver ILI9341 é uma hipótese a testar. Este ensaio não usa Wi-Fi,
LDR, LEDs de RPM, indicador de marchas nem PWM do backlight.

O código deste teste está em `src/diagnostico_tft.cpp`. Selecione o ambiente
`diagnostico_tft` para compilá-lo ou gravá-lo; `src/main.cpp` e o ambiente
padrão `telemetria_f1` agora contêm o teste de recepção dos dados do jogo.

## Antes de ligar

1. Retire o cabo USB do ESP32 antes de fazer ou mudar qualquer ligação.
2. Use os nomes impressos nas placas, sem contar pinos pela posição na foto.
   No ESP32, `D21` corresponde ao GPIO21, por exemplo.
3. Mantenha a placa da tela apoiada em material isolante, sem encostar
   contatos ou soldas em metal. Use fios curtos e conexões firmes.
4. Deixe todos os outros componentes desconectados neste primeiro teste.

## Ligações deste diagnóstico

Esta pinagem substitui a proposta inicial somente para o ensaio. CS e D/C
foram movidos dos GPIOs de inicialização 15 e 2 para GPIO21 e GPIO22.

| Pino impresso na TFT | Ligar no ESP32 | Observação |
| --- | --- | --- |
| VCC | 3V3 | Alimentação inicial conservadora de 3,3 V. |
| GND | GND | Terra comum entre as placas. |
| CS | D21 / GPIO21 | Seleção da TFT. |
| RESET | D4 / GPIO4 | Não conectar ao EN do ESP32. |
| D/C | D22 / GPIO22 | Seleção de comando/dados. |
| SDI | D23 / GPIO23 | MOSI, dados do ESP32 para a TFT. |
| SCK | D18 / GPIO18 | Clock SPI. |
| SDO(MISO) | D19 / GPIO19 | Retorno da TFT para leitura do controlador. |
| LED | D32 / GPIO32 através de um resistor de 220 Ω ou 300 Ω | O firmware mantém o GPIO32 em nível alto; sem PWM. |

O resistor do pino LED pode ficar em qualquer sentido. A ligação é:
`D32 do ESP32 -> resistor -> LED da TFT`.

Assim, o único pino 3V3 do ESP32 fica reservado ao VCC da TFT, sem necessidade
de derivação. Use jumpers FxF para os pinos macho das duas placas. No ramo
do backlight, a montagem pode ser feita com dois jumpers FxF: uma extremidade
no D32, as duas extremidades centrais nas pernas do resistor e a última
extremidade no LED da TFT. Isole as pernas expostas e confira se ficam firmes
nos conectores; fio ou resistor apenas encostado não forma uma ligação segura.

O resistor em série é obrigatório nesse ramo. Com 220 Ω e nível alto de
aproximadamente 3,3 V, a corrente fica limitada a no máximo cerca de 15 mA,
mesmo se o ramo apresentar resistência muito baixa. A iluminação pode ficar
fraca se o pino alimentar o backlight diretamente; isso não indica defeito
da TFT. Esta solução de ensaio não confirma o circuito interno do backlight.
Não use GPIO32 para alimentar o VCC da tela.

Deixe `T_CLK`, `T_CS`, `T_DIN`, `T_OUT` e `T_IRQ` sem conexão. Os quatro
contatos separados `SD_CS`, `SD_MOSI`, `SD_MISO` e `SD_SCK` pertencem ao
cartão SD e também ficam desconectados. Não insira cartão no ensaio.

Não ligue sinais SPI a 5 V e não faça ponte no jumper J1. Existe um regulador
U1 na placa: alimentar VCC com 3,3 V pode resultar em uma tensão interna
menor. Por isso, tela apagada/branca neste ensaio é um resultado inconclusivo.
Se ocorrer, mediremos a saída do regulador e confirmaremos o circuito antes
de mudar a alimentação. Não aplique 5 V por tentativa.

## Compilar e gravar

O ambiente `diagnostico_tft` já está configurado para ESP32 e COM3 no
`platformio.ini`. Se o Windows atribuir outra porta, atualize `upload_port`
e `monitor_port`.

No VS Code, use os comandos Build e Upload do PlatformIO, selecionando
`diagnostico_tft`. As alternativas no terminal do PlatformIO são:

```powershell
pio run -e diagnostico_tft
pio run -e diagnostico_tft -t upload
pio device monitor -p COM3 -b 115200
```

A gravação substitui o programa anterior do ESP32. Depois de gravar, abra
o monitor serial em 115200 baud e pressione EN/RESET no ESP32 para reiniciar
a sequência. Se a gravação parar em `Connecting...`, mantenha BOOT pressionado
durante a conexão e solte quando a escrita começar. Feche o monitor serial
antes de tentar uma nova gravação.

O GPIO32 só será colocado em nível alto após gravar este firmware e executar
o `setup()`. Não espere iluminação a partir desse pino com o programa antigo.

## Resultado esperado

1. Tela inteira vermelha, verde, azul, branca e preta, por 1,5 segundo cada.
2. Quatro orientações com borda, diagonais e indicação do topo e da base.
3. Painel de diagnóstico com resolução, bytes de identificação e contador.

No monitor serial:

- `93` seguido de `41` na leitura de `0xD3`: identificação compatível com
  ILI9341. Confirme também se a imagem é exibida corretamente.
- Apenas `00` ou `FF`, ou outro resultado: identificação inconclusiva. Isso
  pode decorrer da alimentação, MISO, demais ligações ou outro controlador.
- Mensagens de testes enviados não comprovam que a tela recebeu os dados;
  a confirmação da imagem precisa ser visual.

Comandos pelo monitor serial: `t` repete os testes, `i` repete a identificação
e `r` gira a imagem. Após corrigir ligações, reinicie com EN/RESET.

Se a tela não mostrar a sequência, envie a saída serial completa e uma foto
da montagem mostrando os nomes dos pinos usados nas duas placas.

## Referências

- [Perfil ESP32 DOIT DevKit V1 no PlatformIO](https://docs.platformio.org/en/latest/boards/espressif32/esp32doit-devkit-v1.html)
- [Biblioteca Adafruit ILI9341](https://github.com/adafruit/Adafruit_ILI9341)
- [Datasheet ILI9341, comando Read ID4 na página 187](https://cdn-shop.adafruit.com/datasheets/ILI9341.pdf)

A pinagem definitiva e o controle automático de brilho serão definidos
após validar fisicamente a tela e seu circuito de alimentação/backlight.
