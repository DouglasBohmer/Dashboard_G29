# Dashboard de corrida escolhido

Layout escolhido: primeira proposta, com marcha centralizada na TFT.
Revisão de 27/10/2026. O layout agora está implementado no firmware, com
adaptação das fontes e medidas à TFT de 320×240. Veja
[Dashboard no ESP32](../Dashboard_Firmware.md) para o funcionamento atual.

![Dashboard de corrida revisado](corrida-central-v2.png)

Os valores são simulados. A proporção é 4:3, mas a imagem de conceito está
ampliada: fontes e espaçamentos ainda serão ajustados e conferidos na TFT
física de 320×240. O original continua em [corrida-central.png](corrida-central.png).

## Distribuição

- Cabeçalho: posição e volta atual/total.
- Barra de RPM: segmentos progressivos verdes, amarelos e vermelhos ao
  se aproximar da troca de marcha; trechos apagados ficam cinza.
- Centro: marcha grande; velocidade à esquerda.
- Direita: intervalos para os carros imediatamente à frente e atrás na
  classificação, substituindo o antigo bloco de ERS.
- Faixa inferior: DRS ativo em verde; ao lado, duas barras de energia com
  rótulos e percentuais. Logo abaixo das barras, modo do ERS.
- Rodapé: tempo da volta atual, minha melhor volta e melhor volta da prova.
  O último campo substitui o combustível.

## Significado das barras de energia

**VOLTA:** saldo do limite de energia que ainda pode ser aplicado na volta.
É uma medida distinta da carga da bateria. O protocolo informa a energia
aplicada nesta volta em joules, por `m_ersDeployedThisLap`; o percentual
restante é calculado a partir desse valor e do limite de referência do perfil
F1 2025, ainda a validar contra o HUD do jogo.

**BATERIA:** carga armazenada no momento, baseada em `m_ersStoreEnergy`.
O percentual depende da capacidade de referência adequada ao carro.
Não usar uma escala universal sem conferir o carro e o formato do jogo.

A barra VOLTA representa a margem do limite por volta, não uma garantia de
que há essa quantidade armazenada para uso imediato. A aplicação também
depende da carga disponível e das regras do jogo. Não confundir essas barras
com a energia recuperada durante a volta.

`m_ersDeployMode` informa: 0 = nenhum; 1 = médio; 2 = volta rápida/hotlap;
3 = ultrapassagem. O exemplo mostra `MODO: ULTRAPASSAGEM`.

## Origem e tratamento dos dados

| Informação | Origem prevista |
| --- | --- |
| Velocidade, marcha, RPM e DRS ativado | Pacote 6 — Telemetria. |
| Indicação da troca | `m_revLightsPercent`, já lido; calibrar as faixas de cor. |
| Duas barras de energia e modo | Pacote 7 — Estado do carro; percentuais calculados. |
| Posição, volta atual e intervalo à frente | Pacote 2 — Voltas. |
| Total de voltas | Pacote 1 — Sessão. |
| Intervalo atrás | Derivação do pacote 2, usando o intervalo à frente do carro na posição seguinte à do jogador. |
| Minha melhor volta | Histórico válido do jogador, pacote 11. |
| Melhor volta da prova | Menor volta válida nos históricos dos participantes; eventos de volta mais rápida ajudam na atualização. |

O intervalo atrás é uma derivação: o protocolo não traz um campo direto
`deltaToCarBehind`. É preciso conferir posição, estado e validade dos carros.
Casos sem referência, com retardatários ou dados indisponíveis devem receber
tratamento próprio, sem apresentar zero como se fosse um intervalo medido.

Minha melhor volta e melhor volta da prova são campos diferentes. O histórico
do pacote 11 percorre os carros da sessão; verificar os indicadores de volta
válida antes de comparar os tempos. Dados desconhecidos ou antigos aparecem
como `--`, e a mudança de sessão deve limpar os valores anteriores.

## Referências e arquivos

- [Catálogo de dados guardado para uso futuro](../Telemetria_F1_25.md)
- [EA: estruturas UDP F1 25](https://forums.ea.com/t5/s/tghpe58374/attachments/tghpe58374/f1-games-game-info-hub-en/61/5/F1%2025%20Telemetry%20Output%20Structures.txt)
- [EA: especificação completa, histórico de voltas](https://forums.ea.com/t5/s/tghpe58374/attachments/tghpe58374/f1-games-game-info-hub-en/61/4/Data%20Output%20from%20F1%2025%20v3.pdf)
- [Prompt da revisão, feita com imagegen integrada](Prompt_Corrida_v2.md)
