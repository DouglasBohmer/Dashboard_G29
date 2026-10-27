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
| Taxa de envio / UDP Send Rate | 20 Hz para o primeiro teste |
| Formato UDP / UDP Format | 2025 |

Mesmo com o Season Pack 2026, selecione explicitamente o formato 2025.
Outros formatos são recusados nesta versão para não interpretar dados com
estrutura diferente. O jogo já fornece a telemetria do próprio carro, sem
necessidade de tornar públicos os dados dos outros participantes.

Entre em uma sessão e conduza o carro na pista. A troca entre PS5 por cabo
e PS5 por Wi-Fi não exige recompilar; basta garantir que ele alcance o IP
atual do ESP32 e manter as opções UDP.

## Conferir o resultado

- `AGUARDANDO F1 25`: Wi-Fi e receptor UDP prontos, sem telemetria recente.
- `TELEMETRIA ATIVA`: velocidade e RPM reais do carro identificado por
  `m_playerCarIndex`, com atualização da TFT a cada 200 ms.
- `SINAL DE F1 AUSENTE`: não chegaram novos dados do carro por 3 segundos;
  os valores são substituídos por `--` para não apresentar informação antiga.
- `SELECIONE UDP 2025`: chegaram pacotes com outro formato.
- `VERSAO UDP INCOMPATIVEL`: o pacote de telemetria não usa a versão 1 esperada.

`UDP` conta todos os datagramas; `telemetria` conta apenas atualizações do
carro aceitas. O jogo também envia outros tipos de pacote, portanto os dois
contadores normalmente diferem. Tamanhos inválidos, índices fora de 0–21,
marchas inválidas, versões incompatíveis e pacotes duplicados/atrasados são
descartados. As contagens de descarte aparecem no monitor serial.

A marcha é lida e registrada somente no monitor serial neste teste. A TFT
continua reservada a velocidade, RPM e informações de conexão; a exibição
de marcha ficará no display dedicado quando ele estiver disponível.

Comandos seriais: `i` mostra informações atuais; `r` reinicia a tentativa de
conexão às redes cadastradas. O comando `r` não reinicia o ESP32.

Registre, ao testar cada ligação do PS5: local, cabo/Wi-Fi, IP do ESP32,
foto da TFT, trecho do monitor serial e se os valores acompanham o jogo.
O ensaio com o PS5 ainda precisa ser realizado; compilação e pacotes
sintéticos não substituem essa validação de rede e dados reais.

## Verificação do parser

`scripts/testar_parser.ps1` compila 26 verificações estáticas contra o parser
usado no firmware: seleção do primeiro e último carro, velocidade/RPM,
ré/neutro, leitura little-endian, pacotes truncados, índice inválido,
formato/versão e ordenação de quadros com retorno do contador de 32 bits.
Não grava nada no ESP32.

## Fontes

- [EA: especificação UDP e formatos F1 25 / Season Pack 2026](https://forums.ea.com/blog/f1-games-game-info-hub-en/ea-sports%E2%84%A2-f1%C2%AE25-2026-season-pack-udp-specification/12187347)
- [EA: estruturas oficiais F1 25](https://forums.ea.com/t5/s/tghpe58374/attachments/tghpe58374/f1-games-game-info-hub-en/61/5/F1%2025%20Telemetry%20Output%20Structures.txt)
- [Espressif: API Wi-Fi](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/wifi.html)
