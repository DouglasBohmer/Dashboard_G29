# Dashboard F1 25 no ESP32

O ambiente `telemetria_f1` implementa o layout de marcha central na TFT de
320×240. Mantenha a montagem de [Teste_TFT.md](Teste_TFT.md), inclusive o
resistor em série no backlight. O sensor digital de luminosidade usa DO no
GPIO34 e compartilha a alimentação de 3,3 V; veja
[Sensor de luminosidade](Sensor_Luminosidade.md) para a montagem e os testes.
O [Adafruit IO](Adafruit_IO.md) recebe estado do sensor e brilho por MQTT/TLS.

## Campos

- Posição e volta atual/total no cabeçalho.
- Barra progressiva de troca com segmentos verdes, amarelos e vermelhos,
  guiada por `m_revLightsPercent` do jogo. Pode ficar apagada abaixo da
  faixa de indicação de troca, mesmo com o motor funcionando.
- Velocidade à esquerda e marcha grande no centro (`N`, `R`, `1` a `8`).
- Intervalos à frente e atrás à direita; `+1V` indica diferença de uma volta.
- DRS ativado com fundo verde; DRS permitido recebe indicação de pronto;
  desligado fica cinza. São estados diferentes.
- Barras VOLTA e BATERIA, seus percentuais e modo do ERS.
- Tempo atual, melhor pessoal e melhor válida da prova no rodapé.
- Estado da conexão e IP para configurar o destino UDP no jogo.
- Estado claro/escuro e comando percentual do brilho no rodapé conectado.

Dados não recebidos ou vencidos usam `--`. A volta atual inválida aparece em
vermelho. Mudanças de sessão limpam os dados anteriores; flashbacks invalidam
os históricos até a próxima atualização. Os históricos percorrem os carros,
então os melhores tempos podem levar aproximadamente um segundo para chegar.

## Energia

O perfil inicial é para carros de F1 modernos de 2025 (`m_formula == 0`),
com referência de 4.000.000 J para a bateria e para o limite por volta.
As constantes estão em `include/tela_dashboard.h`.

- BATERIA: energia armazenada dividida pela capacidade de referência.
- VOLTA: 100% menos a energia aplicada nesta volta dividida pelo limite.
- Ambos os percentuais ficam limitados ao intervalo 0–100%.

VOLTA é uma estimativa da margem de aplicação por volta, distinta da carga
armazenada e da energia recuperada. A equivalência com a barra do HUD precisa
ser conferida no jogo, especialmente em volta rápida/ultrapassagem. Outros
carros/regulamentos, incluindo 2026, exigem perfil próprio; o formato do pacote
sozinho não determina a capacidade. Quando não há perfil reconhecido, as
porcentagens não são apresentadas. O modo ainda vem diretamente da telemetria.

## Organização e desenho

`include/telemetria_f1.h` lê o pacote 6. `include/dashboard_f1.h` reúne sessão,
voltas, estado do carro, histórico e flashbacks, validando limites, versões,
tamanhos, números não finitos e ordenação por tipo/carros. O intervalo atrás
usa o intervalo à frente do perseguidor encontrado pela posição na prova.

`include/tela_dashboard.h` desenha a interface, com fonte proporcional para
velocidade e marcha. O texto é preparado em um canvas monocromático de 4.320
bytes e convertido em linhas RGB565; somente a união das áreas do texto antigo
e novo é enviada à TFT. Cores e fundo também entram no cache. Barras sobrescrevem
somente os trechos alterados, evitando apagar o campo antes de redesenhar.

O intervalo alvo dos campos principais é 33 ms; dados auxiliares usam 100 ms.
SPI continua em 10 MHz. O alvo não garante 30 quadros completos por segundo:
depende do número de campos alterados, tráfego e tempo gasto no desenho.

## Gravação e verificações

```powershell
pio run -e telemetria_f1 -t upload
pio device monitor -p COM3 -b 115200
```

`i` mostra os dados e contagens de cada pacote; `r` reconecta às redes.
Mantenha UDP 2025, porta 20777 e envio a 60 Hz no F1. O endereço destino deve
ser o IP atual exibido na TFT. Credenciais locais continuam fora do Git.

O ambiente separado `testes_dashboard` contém 58 verificações executadas no
ESP32: pacotes inválidos, floats, DRS, marcha, sequência, expiração, mudança
de sessão, intervalo atrás, retardatários, melhores válidas e flashback.
Em 27/10/2026, todas passaram, além das 26 verificações estáticas existentes.

O dashboard foi gravado na COM3 e recebeu dados reais do PS5 com formato
2025: pacotes de sessão, voltas, carro, energia e histórico. Durante a amostra,
houve variação de velocidade, RPM e marcha; os relatórios registraram picos
de desenho entre 8,6 e 12,1 ms, sem rejeições do parser ou quadros fora de ordem.
O usuário confirmou o carro na pista. A comparação visual de percentuais,
intervalos e fontes com o HUD continua sendo uma verificação manual.

Para repetir a verificação no ESP32, grave `testes_dashboard`, abra a COM3 e
envie `t`. Esse ambiente é uma aplicação de teste; ao terminar, grave novamente
`telemetria_f1` para restaurar o dashboard. `diagnostico_tft` continua disponível.

## Referências

- [EA: estruturas UDP F1 25](https://forums.ea.com/t5/s/tghpe58374/attachments/tghpe58374/f1-games-game-info-hub-en/61/5/F1%2025%20Telemetry%20Output%20Structures.txt)
- [EA: especificação completa](https://forums.ea.com/t5/s/tghpe58374/attachments/tghpe58374/f1-games-game-info-hub-en/61/4/Data%20Output%20from%20F1%2025%20v3.pdf)
