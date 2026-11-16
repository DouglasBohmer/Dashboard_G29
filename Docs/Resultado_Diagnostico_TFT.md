# Diagnóstico da tela branca — 16/11/2026

Montagem atual: TFT ILI9341, CS fixo em GND, RESET fixo em 3V3,
MISO=35, MOSI=23, SCK=18, D/C=22. O ESP32 e a demonstração continuavam
funcionando quando o LCD ficava branco. O comando `t` era recebido pelo
ESP32, mas o usuário informou que a imagem não voltava.

## Bateria executada no ESP32 pela COM3

O firmware com a bateria foi compilado e gravado com verificação de hash.
As 12 verificações estáticas de classificação de leitura foram aprovadas.
O comando `b` foi executado e terminou normalmente, com restauração do
rádio e do brilho. O relatório foi salvo em
`.pio/testes/diagnostico_tft_lote.log`.

| Ensaio | Resultado observado pela serial |
| --- | --- |
| MISO GPIO35 em repouso | 0/64 amostras altas |
| Leituras repetidas a 100 kHz, 500 kHz, 1 MHz e 4 MHz | Todos os registradores lidos como 00 |
| Leitura SPI direta, sem indexação 0xD9 | Todos os bytes lidos como 00 |
| Inicialização a 100 kHz e comandos explícitos de despertar | Leituras continuaram em 00 |
| Wi-Fi e LEDs de sinalização desligados | Leituras continuaram em 00 |
| Backlight comandado a zero temporariamente | Leituras continuaram em 00 |
| SPI por software nos mesmos pinos | Leituras continuaram em 00 |
| Reconstrução do periférico SPI por hardware | Leituras continuaram em 00 |
| Escrita/leitura de MADCTL e RAM RGB | Puladas: ID 9341 não confirmado |

Resumo da bateria: **33 amostras, 33 zeradas, 0 IDs compatíveis e 0 FF**.
Padrões visuais foram enviados; o usuário informou que não conseguiu
observá-los. Portanto, essa execução não confirmou visualmente a ausência
de imagem em todas as etapas. As etapas puladas não contam como testes
de escrita/leitura aprovados nem reprovados.

## Comparação depois de desligar e religar

O usuário relatou que cortar e restabelecer a alimentação faz a imagem voltar,
mesmo sem tocar nas ligações internas. Uma consulta `d`, feita em seguida
sem reinicializar o ESP32/LCD, retornou:

```text
[TFT] CONSULTA | ID=41 00 93 41 | modo=94 | pixel=05
[TFT] ID compativel com ILI9341; a imagem precisa ser conferida.
```

Essa leitura foi salva em `.pio/testes/diagnostico_tft_estado_atual.log`.
Ela demonstra uma resposta compatível do controlador naquele momento,
diferente da bateria anterior. O usuário confirmou que a imagem estava normal
após retirar e recolocar o USB-C no computador, sem tocar no display montado.
Portanto, a comparação entre leitura zerada na falha e ID compatível na partida
normal tem confirmação visual para o estado normal. Os padrões intermediários
da bateria continuam sem confirmação visual, pois o usuário não os observou.

## Interpretação e limites

O resultado favorece investigar a partida/reset do controlador e a sequência
de alimentação. Não exige que alguma conexão tenha sido movimentada: cortar
a alimentação reinicia também a eletrônica do LCD; reenviar comandos SPI
depende de o LCD conseguir recebê-los no estado em que está.

Nesta montagem, o firmware não controla fisicamente CS nem RESET. A biblioteca
reinicializa o LCD por comando, sem aplicar pulso no RESET. Por isso, o fato
de o comando ter sido enviado pelo ESP32 não garante que tenha alcançado ou
recuperado o controlador.

As leituras não medem a tensão de 3V3, não comprovam a integridade de cada
fio e não excluem mau contato, alimentação marginal ou defeito intermitente.
Nenhuma alteração das ligações foi solicitada: os testes foram feitos com a
caixa fechada. Não há confirmação de uma correção definitiva.
