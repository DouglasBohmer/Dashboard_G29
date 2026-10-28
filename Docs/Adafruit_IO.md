# Integração com Adafruit IO

O ambiente `telemetria_f1` publica o estado do sensor instalado e o comando
de brilho da TFT no Adafruit IO, usando PubSubClient 2.8. As redes de casa e
faculdade continuam em `include/configuracao_wifi.local.h`.

## Configuração

As credenciais fornecidas foram salvas em
`include/configuracao_adafruit.local.h`, ignorado pelo Git. Em outra máquina,
copie `include/configuracao_adafruit.exemplo.h` para esse nome e preencha
`USUARIO` e `CHAVE`. Os dois nomes de feed também ficam nesse arquivo.
Recompile e faça Upload após alterar a configuração.

| Feed | Valor enviado | Escala sugerida do gráfico |
| --- | --- | --- |
| `ldr-valor` | Estado confirmado: 0 = claro; 1 = escuro | 0 a 1 |
| `brilho-tft` | Ciclo PWM aplicado, em percentual | 0 a 100 |

O sensor possui saída DO; `ldr-valor` não representa uma medição analógica,
lux ou percentual de iluminação. O brilho permanece em aproximadamente 10%
no escuro e 100% no claro, com valores intermediários durante a transição.
O envio aguarda a confirmação inicial do sensor.

## Comunicação

A conexão usa MQTT com TLS em `io.adafruit.com:8883`, validando o servidor com
o certificado DigiCert Global Root G2 incluído no projeto. O ESP32 sincroniza
o relógio por NTP antes da conexão para verificar a validade do certificado.
Não é usado `setInsecure`.

São enviados dois pontos a cada dez segundos: aproximadamente 12 por minuto.
O limite gratuito é 30 pontos por minuto, compartilhado por todos os clientes
da conta. O intervalo é preservado entre reconexões, inclusive após falha
parcial de envio; dados antigos não são reenviados em rajada.

O MQTT roda em uma tarefa própria no núcleo 0. O loop principal entrega a
amostra atual por uma fila de uma posição e continua lendo o sensor,
controlando o brilho, recebendo o F1 e desenhando a TFT. Somente a amostra
mais recente é mantida; não há histórico local das leituras durante quedas.
Mesmo se o MQTT estiver indisponível, o controle do brilho funciona localmente.

Há uma tentativa de reconexão a cada dez segundos. Os canais `errors` e
`throttle` da conta são acompanhados; um aviso de limite pausa as publicações
por 60 segundos. A mensagem recebida não é reproduzida no log.

O envio usa QoS 0: o contador registra escritas aceitas pelo cliente MQTT,
sem confirmação de persistência pelo broker. Confira os feeds na nuvem
para validar a gravação dos dados.

## Teste após Upload

1. Grave `telemetria_f1` e abra o monitor serial em 115200 baud.
2. Aguarde `Adafruit IO: conectado`.
3. Ilumine e cubra o sensor, mantendo cada condição por pelo menos dez segundos.
4. Confira os dois feeds e o gráfico no Adafruit IO.
5. Envie `i` no serial para consultar estado, pontos enviados, falhas e código MQTT.

Se permanecer aguardando horário NTP, confira o acesso à internet e a
disponibilidade de NTP na rede. Se a conexão falhar, o código MQTT 4 ou 5
indica recusa das credenciais/permissões; -2 indica falha de transporte ou TLS
e -4 indica tempo de resposta excedido. O Monitor do Adafruit IO mostra
avisos sobre os feeds e a conta.

Em 28/10/2026, os dois feeds responderam HTTP 200 e a autenticação MQTT/TLS
foi aceita pelo broker em uma conexão de verificação feita pelo computador.
Nenhum dado de teste foi publicado. O firmware compilou; as 15 verificações
do controle de envio, as 22 do sensor e as 26 do parser passaram. O Upload e
o ensaio de publicação pelo ESP32 continuam pendentes.

## Referências

- [API MQTT e limites do Adafruit IO](https://io.adafruit.com/api/docs/mqtt)
- [PubSubClient](https://github.com/knolleary/pubsubclient)
- [Certificado público DigiCert Global Root G2](https://cacerts.digicert.com/DigiCertGlobalRootG2.crt.pem)
