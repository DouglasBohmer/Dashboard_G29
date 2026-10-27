# Dados disponíveis na telemetria do F1 25

Referência guardada a pedido do usuário em 27/10/2026 para planejar as
próximas funções do projeto. Considera o formato UDP 2025 selecionado no jogo.

| Grupo | Informações disponíveis |
| --- | --- |
| Condução | Velocidade, marcha, RPM, acelerador, freio, embreagem e esterço. |
| DRS | Ativado, permitido e distância para disponibilidade. |
| Corrida | Posição, volta atual, tempos, setores, diferença para o carro à frente e líder. |
| Energia | Combustível, estimativa em voltas, energia e modo do ERS. |
| Pneus | Temperaturas, pressão, composto, desgaste, danos e idade em voltas. |
| Carro | Temperaturas do motor/freios e danos dos componentes. |
| Sessão | Pista, clima, temperaturas, bandeiras, safety car e limite dos boxes. |

A lista descreve dados do protocolo, não funções já implementadas. O firmware
atual interpreta velocidade, RPM, marcha e percentual das luzes de RPM.
Alguns campos variam conforme modo de jogo, carro e permissões de telemetria.

Há ainda movimento, participantes, acerto do carro, eventos, histórico de
voltas, classificação final, lobby, jogos de pneus e tomada de tempo.

Detalhes do layout escolhido e dos cálculos previstos estão em
[Dashboard de corrida](Propostas_Dashboard/Dashboard_Corrida.md).

## Fontes oficiais

- [EA: estruturas UDP F1 25](https://forums.ea.com/t5/s/tghpe58374/attachments/tghpe58374/f1-games-game-info-hub-en/61/5/F1%2025%20Telemetry%20Output%20Structures.txt)
- [EA: especificação completa](https://forums.ea.com/t5/s/tghpe58374/attachments/tghpe58374/f1-games-game-info-hub-en/61/4/Data%20Output%20from%20F1%2025%20v3.pdf)
