# Dashboard F1 2025 no ESP32

O ambiente `telemetria_f1` implementa **Principal F1 2025** e **Pneus e freios
F1 2025** na TFT de 320×240. A montagem final usa CS fixo em GND, RESET fixo em 3V3 e MISO no
GPIO35, com resistor em série no backlight. O sensor digital de luminosidade usa DO no
GPIO34 e compartilha a alimentação de 3,3 V; veja
[Sensor de luminosidade](Sensor_Luminosidade.md) para a montagem e os testes.
O [Adafruit IO](Adafruit_IO.md) recebe estado do sensor e brilho por MQTT/TLS.

## Principal F1 2025

O desenho usa uma grade sobre fundo preto, com a marcha grande no centro.
O topo reúne velocidade, última volta, delta contra a última volta e RPM.
À esquerda fica a posição; à direita, a volta atual/total. Os títulos e o
bloco do DRS acima desses valores foram removidos. A linha abaixo da marcha mostra o intervalo para trás,
o tempo da volta atual e o intervalo para frente. Abaixo de cada intervalo
aparecem posição e nome do piloto, por exemplo `9º - Albon`. Os intervalos são de tempo, em segundos:
positivo para trás e negativo para frente, como na imagem de referência.
Diferenças de voltas continuam identificadas por `V`.

A barra de RPM fica abaixo dos tempos e acompanha os dez LEDs físicos:
1–2 verdes, 3–4 amarelos, 5–7 vermelhos e 8–10 azuis.
Na base ficam as duas barras do ERS, com saldo estimado
da volta, carga da bateria e percentuais. A faixa inferior de 320×24 agora
tem três linhas: fonte `DEMO`/`REAL`/`AGUARDANDO JOGO` e modo ERS; Wi-Fi
conectado (ou rede em tentativa); destino IP/UDP alternado a cada quatro segundos
com `F1 25 > Configuracoes > Telemetria (2025)`. O ERS mantém o fundo cinza
para nenhum, azul para médio, amarelo para volta rápida e vermelho para
ultrapassagem; sem energia válida mostra `ERS: --` em cinza.
As leituras dos sensores continuam no Monitor Serial. O brilho automático continua em 50% no escuro
e 100% no claro.

O delta compara o tempo no mesmo ponto da pista com a **última volta completa
registrada pelo ESP32**. Verde/negativo significa ganho e vermelho/positivo,
perda. O cálculo usa distância da volta e tempo do pacote 2, com interpolação
em 512 trechos. Trata-se de uma estimativa calculada no painel.
Até registrar uma volta completa, o campo mostra `--`. Voltas inválidas,
passagens pelos boxes, lacunas de mais de um segundo/200 m, flashback e mudança
de sessão impedem usar uma referência parcial ou inadequada. Cada volta
completa substitui a referência anterior; não se compara com a melhor volta.

A última volta vem de `m_lastLapTimeInMS`. Os nomes vêm do pacote 4 de
participantes, associado aos vizinhos pela posição no pacote 2, conforme o
[protocolo oficial da EA](https://forums.ea.com/t5/s/tghpe58374/attachments/tghpe58374/f1-games-game-info-hub-en/61/5/F1%2025%20Telemetry%20Output%20Structures.txt).
Para IA, o painel mostra o sobrenome; para humanos, preserva o apelido.
Nomes longos recebem reticências, acentos latinos são convertidos para a fonte
do display e nomes ausentes usam `--`. Líder/último colocado não recebem um
vizinho fictício. As melhores voltas pessoal e da prova continuam no Monitor Serial.

[Prévia do layout](Propostas_Dashboard/Preview_Principal_F1_2025.png), renderizada
com dados de exemplo, coordenadas do firmware e os bitmaps das fontes
Adafruit GFX. A prévia não é uma captura da tela física.

## Tela de pneus e freios

A [prévia de temperaturas](Propostas_Dashboard/Preview_Temperaturas_F1_2025.png)
mostra DE/DD (rodas dianteiras) acima e TE/TD (traseiras) abaixo, do ponto
de vista do piloto. A marcha fica no centro e a velocidade acima dela.
Todos os valores de temperatura são em °C.

Cada pneu tem dois retângulos horizontais: `I` indica temperatura interna,
com fundo sólido colorido; `E` indica temperatura externa/superficial,
com fundo preto e texto colorido. Cada freio tem seu próprio quadro com
fundo que acompanha sua temperatura e texto em contraste. Sem telemetria
recente, os valores ficam `--` e os fundos ficam neutros. A faixa inferior
repete o modo ERS e as cores da principal.

As temperaturas vêm do pacote 6: `m_tyresInnerTemperature`,
`m_tyresSurfaceTemperature` e `m_brakesTemperature`. A ordem no protocolo
é TE, TD, DE, DD; o firmware reorganiza os valores na tela. A temperatura
externa é a da superfície enviada pelo jogo, sem subdivisão entre bordas
interna/central/externa da banda de rodagem.

As faixas iniciais estão em `include/cores_dashboard.h`:

| Cor | Pneu (°C) | Freio (°C) |
| --- | --- | --- |
| Azul | Abaixo de 70 | Abaixo de 300 |
| Ciano | 70–84 | 300–499 |
| Verde | 85–104 | 500–849 |
| Amarelo | 105–114 | 850–999 |
| Vermelho | 115 ou mais | 1000 ou mais |

São limites de apresentação ajustáveis, não alvos universais de desempenho
para todos os compostos, pistas e condições.

## Alternar pelo touchpad

Com o touchpad associado à **Ação UDP 1** no F1 25, cada pressão alterna
`Principal → Pneus e freios → Principal`. Segurar o botão ou pressionar outro
botão junto não repete a ação. Soltar rearma o próximo toque; a Ação UDP 2
não troca páginas. Cópias idênticas do pacote são descartadas.

O evento `BUTN` do pacote 3 contém `buttonStatus`; a Ação UDP 1 é o bit
`0x00100000`, conforme o apêndice de botões da
[especificação da EA](https://forums.ea.com/t5/s/tghpe58374/attachments/tghpe58374/f1-games-game-info-hub-en/61/4/Data%20Output%20from%20F1%2025%20v3.pdf).
O parser detecta a transição solto→pressionado, valida o pacote e separa
a ordenação dos botões dos flashbacks. A troca limpa o cache de desenho,
sem reiniciar a telemetria, o delta, os sensores ou os LEDs.

### Organização para novas telas

`include/perfis_dashboard.h` identifica jogo, categoria, página, formato UDP
e fórmula do carro. O catálogo contém `f1-2025/principal` e
`f1-2025/temperaturas`, alternados por `PerfisDashboard::Selecao`.
Os nomes dos perfis aparecem somente no Monitor Serial.
`include/telas/principal_f1_2025.h` e `include/telas/temperaturas_f1_2025.h`
contêm os desenhos das páginas;
`include/tela_dashboard.h` reúne os formatadores compartilhados.
Novas páginas e perfis devem ser registrados depois de implementados.
F2, F1 2026 e outros jogos ainda não estão disponíveis. Uma fórmula
incompatível mostra `PERFIL INDISPONIVEL` e mantém dados/LEDs do perfil suspensos.

## Demonstração automática sem jogo

O ambiente normal `telemetria_f1` entra em demonstração após **10 segundos
sem receber um pacote F1 2025 válido**, mesmo com Wi-Fi conectado. O prazo
começa quando a inicialização termina e é renovado a cada pacote real aceito.
Sem redes cadastradas, a demonstração também funciona.

O ciclo de 45 segundos se repete nesta ordem:

| Tempo no ciclo | Ação |
| --- | --- |
| 0–8 s | Acelera de 60 a 300 km/h, com marchas e RPM variando. |
| 8–12 s | Freia até 85 km/h e aquece os freios. |
| 12–16 s | Bandeira amarela: LEDs amarelos piscam. |
| 16–20 s | Bandeira verde e retomada da aceleração. |
| 20–23 s | DRS disponível, ainda fechado. |
| 23–28 s | DRS aberto e aceleração até 330 km/h. |
| 28–33 s | Bandeira azul: os dois brancos piscam. |
| 33–45 s | Troca para pneus/freios e varia as temperaturas por todas as cores. |

Posição, voltas, tempos, intervalos, nomes dos vizinhos, modos ERS, bateria
e histórico também recebem valores fictícios. Uma volta virtual leva
30 segundos reais; após a primeira volta completa, o delta mostra ganho
e perda nas voltas seguintes. Sensores LDR/DHT22 e publicação Adafruit IO
continuam com as leituras físicas.

`include/demonstracao_dashboard.h` gera pacotes locais para uma instância
separada do mesmo parser usado pelo jogo. Esses pacotes não são transmitidos
pela rede e não entram nas estatísticas da recepção real. Wi-Fi, escolha de
IP e UDP continuam funcionando durante toda a demonstração.
O primeiro pacote real aceito encerra a demonstração imediatamente e
restaura a página que estava selecionada antes dela, incluindo a Ação UDP 1
se recebida nesse pacote. Os campos reais ainda não recebidos ficam `--`.
Uma nova ausência de 10 segundos reinicia a demonstração do começo.

Nas duas páginas, o rodapé identifica `DEMO` e a etapa, mostra o SSID real
conectado ou em tentativa e alterna IP/porta com o caminho no jogo.
Em **F1 25 > Configurações > Telemetria**, habilite a saída UDP, use o
IP mostrado, porta **20777** e formato **2025**. ESP32 e jogo precisam
estar na mesma rede local; o ESP32 usa Wi-Fi de 2,4 GHz.

Para verificar a lógica sem gravar o ESP32:

```powershell
./scripts/testar_demonstracao.ps1
pio run -e telemetria_f1 -e testes_dashboard
```

O primeiro comando executa o ciclo, prioridade dos dados reais, reinício,
isolamento das sessões e virada de `millis()` no computador. Requer g++,
Zig ou Visual Studio Build Tools com C++ e Windows SDK.

## Teste sequencial dos LEDs

O teste está **desativado** por `TESTE_LEDS_SEQUENCIAL = false` em `src/main.cpp`.
Os LEDs agora seguem a telemetria. Para repetir o teste, altere a constante para `true`.
Após a inicialização, acende um canal por vez por 0,3 segundo (300 ms), apagando o
anterior. Primeiro percorre os dez LEDs superiores da esquerda para a direita,
olhando o painel pela frente. Depois acende o branco esquerdo, o branco direito,
o par amarelo e o par verde. O ciclo de 4,2 segundos se repete automaticamente.
Os 16 LEDs físicos estão incluídos em 14 etapas, pois cada par lateral
compartilha um GPIO e acende junto.

O teste funciona sem Wi-Fi e sem telemetria do jogo. Durante cada etapa,
o canal recebe HIGH contínuo; as bandeiras não piscam nesse modo. O Monitor
Serial identifica o LED e o GPIO. A TFT mantém a página selecionada.
Para restaurar o acionamento por RPM, DRS e bandeiras, altere a constante
para `false` e grave o firmware novamente.

## Montagem final de 12/11/2026

| Componente | GPIO ou ligação |
| --- | --- |
| TFT D/C, MOSI, SCK, MISO | 22, 23, 18, 35 |
| TFT CS e RESET | GND e 3V3, respectivamente |
| Iluminação TFT | GPIO32 com resistor em série e PWM |
| LDR DO | GPIO34 em modo INPUT |
| DHT22 OUT | GPIO33, alimentação de 3,3 V |
| LEDs RPM 01–10 | 13, 14, 16, 17, 25, 27, 26, 4, 19, 21 |
| DRS disponível e DRS aberto | GPIO5 e GPIO15 |
| Dois amarelos laterais | Um canal no GPIO2 |
| Dois verdes laterais | Um canal no GPIO12 |

Os LEDs são ativos em HIGH e têm os negativos unidos ao GND. A montagem
informada usa um resistor por LED de RPM/DRS e um resistor compartilhado
por cada par lateral, totalizando 15 resistores com o backlight.
O par amarelo chega a K07 pelo resistor K11–K07 e o verde chega a O07
pelo resistor O11–O07. Os pontos M11/M07 e Q11/Q07 ficam livres.
Os resistores instalados podem ser de 220 Ω ou 300 Ω.

A biblioteca Adafruit ILI9341 aceita CS=-1 e RESET=-1. O firmware usa tipos
assinados, inicializa SPI com os pinos personalizados e executa reset por
comando. GPIO16 e GPIO17 acionam LEDs, e a comunicação serial utiliza Serial.

Os dez LEDs superiores seguem `m_revLightsPercent` do pacote 6 do jogo,
dividido em dez segmentos cumulativos. Não usam mais a escala fixa de RPM
de bancada. O display usa a mesma quantidade de segmentos e cores dos LEDs.
A pinagem conserva a correção dos LEDs 6 e 7: GPIO27 e GPIO26, respectivamente.
O branco esquerdo acende quando `m_drsAllowed` ou `m_drs` está ativo.
O direito segue `m_drs`, permitindo manter os dois acesos com o DRS aberto.
As bandeiras vêm de `m_vehicleFiaFlags` do carro do jogador no pacote 7.
Valor 3 faz os amarelos piscarem e valor 1 faz os verdes piscarem.
Cada fase dura 500 ms, alternando aceso e apagado a cada meio segundo.
Valor 2 (bandeira azul) faz os dois brancos do DRS piscarem juntos, com
prioridade sobre os estados disponível/aberto. Ao sair da bandeira azul,
os brancos retomam o comportamento normal do DRS.
Os canais usam a validade dos respectivos pacotes, de três segundos.
Desconexão, pausa informada pelo jogo e expiração dos dados apagam os
indicadores correspondentes.

Todos os canais dos LEDs recebem LOW logo no início de setup(), inclusive
nos ambientes de diagnóstico e testes. Durante reset e bootloader, os níveis
dos GPIO2/5/12/15 dependem do hardware e dos estados de inicialização do ESP32.

O DHT22 é lido a cada 2,5 segundos com DHTesp 1.19.0. Temperatura em °C,
umidade em porcentagem e falhas aparecem no Monitor Serial a 115200 baud.
As leituras dos sensores ficam no Monitor Serial; a faixa inferior da TFT
é reservada ao modo ERS.
O GPIO33 usa INPUT e depende do pull-up físico do módulo. Se houver falhas,
o pull-up externo previsto é de 10 kΩ entre OUT (A06) e 3V3 (A21).
Uma leitura inválida é sinalizada no relatório e uma nova tentativa ocorre
no próximo intervalo. O LDR e seu controle automático de brilho funcionam
durante as tentativas de leitura do DHT22.

O firmware foi adaptado à montagem informada. A gravação e a validação
física da pinagem final, dos indicadores e do DHT22 precisam ser executadas.

## Campos

- Posição à esquerda e volta atual/total à direita da marcha.
- Velocidade, última volta, delta contra a última volta e RPM no topo.
- Barra progressiva de troca com dois segmentos verdes, dois amarelos,
  três vermelhos e três azuis,
  guiada por `m_revLightsPercent` do jogo. Pode ficar apagada abaixo da
  faixa de indicação de troca, mesmo com o motor funcionando.
- Marcha grande no centro (`N`, `R`, `1` a `8`).
- Intervalo e posição/nome do carro de trás à esquerda, tempo atual no centro
  e intervalo e posição/nome do carro da frente à direita;
  `+1V` ou `-1V` indica diferença de uma volta.
- Barras VOLTA e BATERIA, seus percentuais e faixa de modo ERS colorida.
- DRS nos LEDs brancos; bandeira azul pisca os dois juntos.
- IP, conexão, brilho, temperatura e umidade no Monitor Serial.

Dados não recebidos ou vencidos usam `--`. A volta atual inválida aparece em
vermelho. Mudanças de sessão limpam os dados anteriores; flashbacks invalidam
os históricos até a próxima atualização. Os históricos percorrem os carros,
então os melhores tempos podem levar aproximadamente um segundo para chegar.

## Energia

O perfil inicial é para carros de F1 modernos de 2025 (`m_formula == 0`),
com referência de 4.000.000 J para a bateria e para o limite por volta.
As constantes estão em `include/telas/principal_f1_2025.h`.

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
voltas, participantes, estado do carro, histórico e flashbacks, validando limites, versões,
tamanhos, números não finitos e ordenação por tipo/carros. O intervalo atrás
usa o intervalo à frente do perseguidor encontrado pela posição na prova.

As páginas de `include/telas/` desenham a interface, com fonte proporcional para
velocidade e marcha. O texto é preparado em um canvas monocromático de 4.320
bytes e convertido em linhas RGB565; somente a união das áreas do texto antigo
e novo é enviada à TFT. Um único `CamposBufferizados` compartilha o canvas
e o cache entre páginas. Cores e fundo também entram no cache. Barras sobrescrevem
somente os trechos alterados, evitando apagar o campo antes de redesenhar.

O intervalo alvo dos campos principais é 33 ms; dados auxiliares usam 100 ms.
SPI continua em 10 MHz. O alvo não garante 30 quadros completos por segundo:
depende do número de campos alterados, tráfego e tempo gasto no desenho.

## Gravação e verificações

### Endereço do painel

A preferência é `192.168.8.80`, com quatro alternativas: `192.168.8.81`,
`.82`, `.83` e `.84`. O ESP32 descobre gateway e máscara por DHCP e verifica
os candidatos por ARP antes de fixar um endereço. Se um candidato responder
como ocupado, tenta o seguinte. O jogo deve usar **o endereço efetivamente
mostrado no Monitor Serial**, com porta `20777`. O relatório de rede identifica
a opção escolhida. A faixa inferior da TFT mostra somente o modo ERS.

O endereço precisa pertencer à rede local: na rede `192.168.1.x/24`, por
exemplo, as opções passam a `192.168.1.80` até `192.168.1.84`. Na rede
`192.168.8.x/24`, permanece exatamente a faixa preferida. O firmware não altera
o roteador nem os endereços dos demais aparelhos. `192.168.1.18` não é usado
porque foi identificado como o endereço deste computador.

Há quatro consultas por candidato, espaçadas em 500 ms. Falha na consulta,
endereço do gateway e candidato fora da sub-rede fazem a seleção avançar.
Se nenhum dos cinco puder ser confirmado, usa o IP concedido pelo roteador e
mostra esse destino real. ARP detecta aparelhos que respondem naquele momento;
não reserva o endereço contra aparelhos desligados ou futuras concessões DHCP.
Para manter uma reserva permanente, associe o endereço escolhido ao MAC do
ESP32 no roteador.

### Compilação e teste

```powershell
pio run -e telemetria_f1 -t upload
pio device monitor -p COM3 -b 115200
```

O monitor inicia com os relatórios contínuos desativados. `i` mostra os dados
e contagens de cada pacote uma única vez; `r` reconecta às redes. `v` alterna
os relatórios automáticos a cada dois segundos e os avisos de etapas da demo.
`t` desativa os relatórios contínuos, reenvia a inicialização do LCD e redesenha a página atual, sem reiniciar
o ESP32 nem limpar a sessão/demonstração. A tentativa usa 1 MHz para comandos
e desenho até reiniciar, e informa ID, modo de alimentação e formato de pixel
antes/depois. `d` somente consulta esses registradores a 1 MHz, preservando a
frequência de desenho. As leituras podem ser inconclusivas.
Ao ligar, o firmware espera 500 ms adicionais, inicializa a 1 MHz e depois
desenha a 10 MHz. Veja [Tela branca ao ligar](Teste_TFT.md)
para testar sem abrir a caixa; a recuperação física ainda precisa ser validada.
As respostas do teste usam o prefixo `[TFT]`. As mensagens de conexão e falhas
continuam disponíveis; a demonstração e o jogo continuam funcionando ao
desativar os relatórios.
`b` executa a bateria de diagnóstico completa (velocidades, leitura direta,
inicialização, padrões RGB, comparação de carga e SPI por software). Durante
essa bateria, o processamento normal e o MQTT pausam; `x` cancela. O relatório
usa `[TFT-B]` e o estado dos periféricos é restaurado ao terminar. Veja os
detalhes e a captura automática em [Teste TFT](Teste_TFT.md).
Mantenha UDP 2025, porta 20777 e envio a 60 Hz no F1. O endereço destino deve
ser o IP atual informado no Monitor Serial. Credenciais locais continuam fora do Git.

O ambiente separado `testes_dashboard` contém 141 verificações: pacotes
inválidos, floats, DRS, marcha, sequência, expiração, mudança de sessão,
intervalos, retardatários, participantes, ultrapassagens, delta por distância,
melhores válidas, flashback, perfil de jogo/carro, seleção/formatação do IP,
temperaturas, cores e botão de troca de telas.
Em 13/11/2026, a suíte foi executada no computador com adaptação de String/Serial,
e compilada para ESP32. A validação física dos novos campos continua pendente.

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
