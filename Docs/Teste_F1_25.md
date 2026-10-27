# Teste de telemetria F1 25 no PS5

## Cabo e Wi-Fi no PS5

As duas opções usam o mesmo firmware:

| Local e ligação do PS5 | ESP32 | Condição |
| --- | --- | --- |
| Casa: PS5 por cabo ao roteador | Wi-Fi de 2,4 GHz desse roteador | Os dois precisam se alcançar pela rede local. |
| Faculdade: PS5 por Wi-Fi | Wi-Fi de 2,4 GHz da mesma rede local | A rede precisa permitir comunicação UDP entre os dispositivos. |

O ESP32 deste projeto sempre usa Wi-Fi. Não existe um seletor cabo/Wi-Fi no
firmware, pois a forma de conexão do PS5 não muda os pacotes UDP recebidos.
O PS5 pode usar Wi-Fi de 5 GHz se o roteador fizer a ponte com a rede do ESP32.

Ter acesso à Internet na faculdade não garante acesso entre os aparelhos.
Rede de visitantes, isolamento de clientes, VLANs ou bloqueio de UDP podem
impedir o teste. SSID e senha comum são suportados nesta versão. Portal
cativo, eduroam ou autenticação institucional por usuário/certificado precisam
ser avaliados com a TI antes de usar essa rede. Como alternativa de bancada,
pode-se usar uma rede particular de 2,4 GHz que permita tráfego entre clientes.

## Montagem já validada

Use exatamente as ligações de [Teste_TFT.md](Teste_TFT.md), incluindo o resistor
de 220 Ω ou 300 Ω entre D32 e LED. Nenhum fio precisa ser acrescentado para
receber UDP. Preserve as conexões isoladas e desconecte o USB ao mexer nelas.

A TFT já foi validada manualmente pelo usuário: cores, rotação e imagem em
320×240 funcionaram, com leitura `41 00 93 41`, compatível com ILI9341.

## Cadastrar casa e faculdade

Abra `include/configuracao_wifi.local.h`. Esse arquivo já foi criado localmente
com campos vazios e é ignorado pelo Git. Se estiver usando um clone novo,
copie `include/configuracao_wifi.exemplo.h` para esse nome.

Preencha as entradas desta forma, mantendo as aspas:

```cpp
constexpr Rede REDES[] = {
    {"NOME_WIFI_CASA", "SENHA_WIFI_CASA"},
    {"NOME_WIFI_FACULDADE", "SENHA_WIFI_FACULDADE"},
};
```

Deixe `{"", ""}` na segunda linha enquanto não souber a rede da faculdade.
Uma rede sem senha pode ser cadastrada com nome preenchido e senha vazia.
Não preencha as credenciais no arquivo de exemplo, que é publicado no GitHub.

O firmware tenta as redes preenchidas na ordem, aguardando até 15 segundos
por tentativa, e volta à primeira se nenhuma conectar. Uma conexão válida é
mantida; o firmware não troca de rede apenas porque outra está mais forte.
Após desconexão, a busca pelas redes cadastradas é retomada automaticamente.
Adicionar ou alterar uma rede exige recompilar e gravar novamente.

### Rede com WPA antigo

O padrão é exigir WPA2 para redes com senha. Se o roteador usar apenas WPA
com TKIP, o ESP32 pode encontrar o nome da rede e ainda assim não conectar.
Prefira WPA2 com AES nas configurações do roteador. Para compatibilidade
com uma rede WPA existente, o campo opcional `permitirWpaLegado` deve ser
`true` somente na entrada dessa rede:

```cpp
struct Rede {
  const char *nome;
  const char *senha;
  bool permitirWpaLegado = false;
};

constexpr Rede REDES[] = {
    {"NOME_WIFI_CASA", "SENHA_WIFI_CASA", true},
    {"NOME_WIFI_FACULDADE", "SENHA_WIFI_FACULDADE"},
};
```

Arquivos locais criados antes dessa opção precisam incluir o novo campo
na estrutura `Rede`. As entradas com dois valores continuam exigindo WPA2.
Quando o roteador passar a WPA2/AES, remova o `true` e grave novamente.
O modo de compatibilidade aceita WPA, sem habilitar WEP. A biblioteca
também permite redes abertas cadastradas com senha vazia.

Se a TFT permanecer em `CONECTANDO WI-FI` e `IP: --`, confira o monitor
serial: ele registra o código e uma indicação do motivo da desconexão.
Falhas de autenticação também podem ocorrer por configuração do roteador;
o código sozinho não comprova senha incorreta.

Ensaio de bancada em 27/10/2026: a rede de 2,4 GHz usava WPA/TKIP e a TFT
permanecia sem IP com o limite padrão WPA2. Após habilitar compatibilidade
somente nessa entrada, o firmware foi compilado, gravado pela COM3 e a
conexão com IP via DHCP foi confirmada no monitor serial durante 18 segundos.
A recepção de telemetria do PS5 foi confirmada pelo usuário no teste seguinte.

## Gravar o teste de rede

No PlatformIO, selecione o ambiente **telemetria_f1**:

```powershell
pio run -e telemetria_f1
pio run -e telemetria_f1 -t upload
pio device monitor -p COM3 -b 115200
```

A porta configurada é COM3. O firmware mostra o IP obtido por DHCP na TFT e
no monitor serial. Anote o IP exibido após conectar. Ele pode mudar ao trocar
de local ou reconectar; nesse caso, atualize o destino no jogo.

Sem redes cadastradas, a TFT informa `CONFIGURE O WI-FI`. Isso é esperado
até preencher o arquivo local e gravar novamente.

Para voltar ao teste anterior, selecione **diagnostico_tft** e faça Upload:

```powershell
pio run -e diagnostico_tft -t upload
```

## Configurar o F1 25 no PS5

Em Configurações / Telemetria, use:

| Opção | Valor |
| --- | --- |
| Telemetria UDP / UDP Telemetry | Ativada / On |
| Modo de transmissão / UDP Broadcast Mode | Desativado / Off |
| Endereço IP UDP / UDP IP Address | IP mostrado na TFT do ESP32 |
| Porta UDP / UDP Port | 20777 |
| Taxa de envio / UDP Send Rate | 60 Hz para maior fluidez; 20 Hz pode ser usado no diagnóstico inicial |
| Formato UDP / UDP Format | 2025 |

Mesmo com o Season Pack 2026, selecione explicitamente o formato 2025.
Outros formatos são recusados nesta versão para não interpretar dados com
estrutura diferente. O jogo já fornece a telemetria do próprio carro, sem
necessidade de tornar públicos os dados dos outros participantes.

Entre em uma sessão e conduza o carro na pista. A troca entre PS5 por cabo
e PS5 por Wi-Fi não exige recompilar; basta garantir que ele alcance o IP
atual do ESP32 e manter as opções UDP.

## Conferir o dashboard

O ambiente padrão agora mostra o dashboard completo. A marcha fica no centro
da TFT, acompanhada por velocidade, RPM colorido, DRS, intervalos, energia e
tempos. Veja [Dashboard no ESP32](Dashboard_Firmware.md) para as escalas e
o significado de cada campo.

- `F1 ATIVO`: chegaram dados recentes do carro do jogador.
- `AGUARDANDO F1`: Wi-Fi e UDP prontos; sem dados recentes. Campos vencidos
  ficam com `--`, e o IP permanece no rodapé.
- `SELECIONE UDP 2025 NO F1`: chegaram pacotes com outro formato.
- `VERSAO UDP INCOMPATIVEL`: um pacote utilizado tem versão não suportada.
- DRS verde: ativado. DRS pronto: permitido, ainda sem ativação.
- Tempo atual vermelho: volta marcada como inválida.
- `+1V` nos intervalos: diferença de volta, em vez de segundos.
- Melhor pessoal/prova: dependem da chegada dos históricos válidos.

A barra de RPM usa a indicação de troca fornecida pelo jogo e pode ficar
apagada abaixo dessa faixa. A barra VOLTA é uma estimativa do saldo de
aplicação de energia, com perfil F1 moderno 2025; confira com o HUD antes de
usar outros carros ou regulamentos.

Os contadores ficam no monitor serial. `UDP` conta todos os datagramas;
`atualizados` conta pacotes utilizados. Há contagem por tipo: sessão, voltas,
carro, energia e histórico. Quadros duplicados/antigos e pacotes inválidos
são registrados separadamente. `i` mostra informações; `r` limpa os dados e
reinicia a conexão às redes cadastradas.

## Fluidez e verificações

Os campos principais têm intervalo alvo de 33 ms e os auxiliares, 100 ms.
O texto é preparado na RAM e apenas as áreas alteradas são transmitidas.
Valores iguais não são redesenhados; mudanças de cor também são consideradas.
SPI continua em 10 MHz. A inspeção visual da TFT deve confirmar legibilidade
e ausência das piscadas de apagamento.

Em 27/10/2026, passaram as 26 verificações estáticas de
`scripts/testar_parser.ps1` e 58 verificações executadas no ESP32 pelo
ambiente separado `testes_dashboard`. Após o teste, foi gravado novamente o
ambiente `telemetria_f1`.

Um ensaio adicional por UDP/Wi-Fi com valores simulados a 60 Hz resultou em
1.459 pacotes aceitos, sem rejeições ou quadros fora de ordem nesse período.
Os relatórios durante o movimento mostraram picos de desenho de 26,2 a
31,7 ms; a primeira atualização chegou a 35,2 ms e a limpeza/reconexão, a
46,4 ms. O ensaio não mede ausência de perdas UDP na rede nem garante esse
desempenho em todas as situações. Os dados simulados foram limpos ao final.

Após limpar a simulação, o usuário confirmou o carro na pista e chegaram
pacotes reais do PS5 de todos os tipos utilizados. Velocidade, RPM e marcha
variaram; energia, modo e históricos também foram recebidos. Os relatórios
da amostra mostraram picos de desenho de 8,6 a 12,1 ms, sem rejeições ou
quadros fora de ordem. A comparação visual de todos os novos campos com o
HUD e o teste na rede da faculdade ainda devem ser realizados.

## Fontes

- [EA: especificação UDP e formatos F1 25 / Season Pack 2026](https://forums.ea.com/blog/f1-games-game-info-hub-en/ea-sports%E2%84%A2-f1%C2%AE25-2026-season-pack-udp-specification/12187347)
- [EA: estruturas oficiais F1 25](https://forums.ea.com/t5/s/tghpe58374/attachments/tghpe58374/f1-games-game-info-hub-en/61/5/F1%2025%20Telemetry%20Output%20Structures.txt)
- [Espressif: API Wi-Fi](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/wifi.html)
- [Adafruit: redução de piscadas com buffers de desenho](https://learn.adafruit.com/adafruit-gfx-graphics-library/minimizing-redraw-flicker)
