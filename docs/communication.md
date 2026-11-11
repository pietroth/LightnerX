# Comunicação e protocolo INER

## INER e LightnerX

INER é um protocolo semântico. Ele define o significado e a organização das mensagens, mas não define sockets, endereçamento, threads ou o mecanismo de transmissão.

O LightnerX implementa os tipos de mensagem do protocolo e oferece componentes de rede baseados em GameNetworkingSockets. A divisão é:

| Responsabilidade | INER | LightnerX |
| --- | --- | --- |
| Significado de Event, Response, Intention e Notification | Sim | Representado pelos tipos C++ |
| Identificação de mensagens | Sim | Campos `MBT::type`, `correlationId` e `status` |
| Codificação dos cabeçalhos | Sim | Estruturas empacotadas e conversores |
| Sockets e conexões | Não | `Server`, `Client` e GameNetworkingSockets |
| Modelo de execução | Não | Escolhido pela aplicação |

## Papéis

### Autoridade

A Autoridade controla estados internos. Ela recebe `Intention` e `Notification` do Obediente e envia `Event` e `Response`.

No LightnerX, o servidor normalmente interpreta mensagens recebidas com:

```cpp
interpreter.obedientInterpret(bytes, registry, connection, connections);
```

### Obediente

O Obediente envia intenções à Autoridade e recebe informações sobre acontecimentos e resultados.

No LightnerX, o cliente normalmente interpreta mensagens recebidas com:

```cpp
interpreter.authorityInterpret(bytes);
```

## Tipos de mensagem

### Event

`Event` comunica um acontecimento da Autoridade. Seu cabeçalho contém:

- `BaseProtocol::size`;
- `MBT::type`, usado como `type_id`.

O payload, quando presente, vem depois do cabeçalho.

### Response

`Response` relaciona o resultado a uma intenção por meio de `correlationId` e contém um `status`.

Os status são expostos pelas constantes do módulo `lightnerx.communication.response`:

| Constante | Valor | Significado | Tipo usado pelo LightnerX |
| --- | ---: | --- | --- |
| `APPROVED_RESPONSE` | `0` | intenção aprovada integralmente | `Response` |
| `DENIED_RESPONSE` | `1` | intenção negada | `UnsuccessfulResponse` |
| `PARTIALLY_APPROVED_RESPONSE` | `2` | intenção parcialmente aplicada | `UnsuccessfulResponse` |
| `FAILURE_RESPONSE` | `3` | erro interno durante o processamento | `Response` |

Uma falha não significa necessariamente que a intenção foi negada; a aplicação pode decidir tentar novamente. `UnsuccessfulResponse` acrescenta `reasonCode` ao cabeçalho de respostas negadas ou parcialmente aprovadas. Na aprovação parcial, o significado do payload é definido pela aplicação.

### Intention

`Intention` solicita que a Autoridade execute uma ação. Seu cabeçalho contém:

- `BaseProtocol::size`;
- `MBT::type`;
- `correlationId`.

O payload da intenção vem depois desse cabeçalho.

### Notification

`Notification` comunica uma informação à Autoridade sem exigir validação de estado, pois não solicita alteração dos estados internos da Autoridade. Seu cabeçalho contém `size` e `type`, seguido pelo payload opcional.

No servidor, o `NotificationRegistry` informa quais `type_id` devem ser interpretados como `Notification`:

```cpp
import std;
import lightnerx.communication.notification;

void registerNotifications(lightnerx::communication::NotificationRegistry& registry) {
    registry.registerId(47);
    registry.registerIds(std::unordered_set<std::uint16_t>{99, 100});
}
```

Os valores de `type_id` são definidos pela aplicação. Não existe uma tabela universal de tipos INER. Na recepção de mensagens do Obediente, os IDs registrados identificam notificações; os demais identificam intenções. Para enviar notificações, use `notification2OutboundMessage`.

## Representação binária

As estruturas de cabeçalho são empacotadas sem padding adicional por meio de `#pragma pack(push, 1)`. Os conversores copiam os campos na representação nativa da máquina; os participantes devem usar uma representação compatível para esses campos e combinar o formato do payload.

### Mensagens da Autoridade

O primeiro campo usa um valor combinado: o bit menos significativo indica se a mensagem é um `Response`, e os quinze bits restantes representam o tamanho.

| Mensagem | Cabeçalho | Conteúdo |
| --- | --- | --- |
| `Event` | 2 bytes de `size` combinado + 2 bytes de `type` | payload variável |
| `Response` | 2 bytes de `size` combinado + 2 bytes de `correlationId` + 1 byte de `status` | sem payload para os status representados por `Response` |
| `UnsuccessfulResponse` | estrutura de `Response` + 2 bytes de `reasonCode` | payload variável para resposta parcial |

Na construção do cabeçalho, o valor combinado segue a forma:

```text
Event:    (tamanho_total << 1) | 0
Response: (tamanho_total << 1) | 1
```

O interpretador recupera o tamanho com `base >> 1` e distingue `Event` de `Response` com `base & 1`. Nos cabeçalhos entregues ao callback, `protocol.size` já contém o tamanho decodificado, sem o bit de tipo.

### Mensagens do Obediente

Nas mensagens do Obediente, `size` representa diretamente o tamanho total da mensagem.

| Mensagem | Cabeçalho | Conteúdo |
| --- | --- | --- |
| `Notification` | 2 bytes de `size` + 2 bytes de `type` | payload variável |
| `Intention` | 2 bytes de `size` + 2 bytes de `type` + 2 bytes de `correlationId` | payload variável |

O tamanho informado deve incluir o cabeçalho e o payload. Os conversores copiam o cabeçalho e o payload para um `SteamNetworkingMessage_t`; eles não montam o valor semântico do campo `size` automaticamente.

## Identificadores

- `type_id` identifica o tipo semântico definido pela aplicação e aparece no código como `MBT::type`.
- `correlationId` relaciona uma resposta com a intenção correspondente.
- O ID recebido no callback de mensagens interpretadas pelo servidor é o identificador interno atribuído pelo `ConnectionManager`.

Esse ID interno pertence à implementação de gerenciamento de conexões e não é um campo do protocolo INER. O nome `originId` também não designa um campo do protocolo. Destinos de rede usam handles `HSteamNetConnection`, distintos dos IDs internos e dos identificadores semânticos.

## Destinos de rede

`lightnerx::network::Target`, exportado pelo módulo `lightnerx.network.target`, define para onde uma mensagem enfileirada será enviada. As fábricas estáticas criam a descrição do destino:

| Fábrica | Destino |
| --- | --- |
| `Target::single(connection)` | uma conexão específica |
| `Target::group(connections)` | todas as conexões presentes no vetor informado |
| `Target::broadcast()` | todas as conexões registradas no `ConnectionManager` |
| `Target::broadcast(exceptions)` | todas as conexões registradas no `ConnectionManager`, exceto os handles informados |

`Target::group` guarda a seleção explícita de handles. O broadcast consulta o `ConnectionManager` passado a `flushAll` no momento do flush, usando as associações mantidas pela aplicação. Suas exceções também são handles `HSteamNetConnection`.

## Fluxo de recebimento

```text
Rede -> callback do Server -> obedientInterpret -> callback da aplicação
Rede -> callback do Client -> authorityInterpret -> callback da aplicação
```

`MessageInterpreter` entrega um `lightnerx::communication::Message` cujo `header` contém o tipo concreto em uma `std::variant` e cujo `payload` é um `std::span<const std::byte>`.

Os callbacks são executados durante a chamada de interpretação. O cabeçalho é entregue por valor, mas o span apenas referencia os bytes recebidos. No fluxo com `Server` ou `Client`, esses bytes são liberados depois dos callbacks de recebimento. Para conservar o payload, copie seus bytes durante o callback; copiar apenas `Message` mantém a referência ao mesmo buffer.

## Fluxo de envio

O fluxo comum é:

1. Inicializar a rede e chamar `initMessagesBatcher(capacity)` antes do primeiro enfileiramento.
2. Construir o cabeçalho com o tamanho total da mensagem.
3. Criar um `lightnerx::network::Target`.
4. Usar o conversor correspondente para obter um `lightnerx::network::OutboundMessage`.
5. Chamar `queueMessage(bytes, std::move(outbound.target))`, que copia os bytes para a fila.
6. Liberar a mensagem original com `outbound.message->Release()`, inclusive se o enfileiramento retornar erro.
7. Verificar o resultado do enfileiramento e chamar `flushAll(connectionManager)` no momento escolhido pela aplicação.

Exemplo de um `Event` da Autoridade com um byte de payload definido pela aplicação. A função pressupõe a rede e o batcher inicializados e permite broadcast com ou sem exceções:

```cpp
#include "steam/steamnetworkingtypes.h"

import std;
import lightnerx.communication.event;
import lightnerx.communication.message;
import lightnerx.network.connection_manager;
import lightnerx.network.message_batch;
import lightnerx.network.target;

std::expected<void, std::error_code> broadcastEvent(
    const lightnerx::network::ConnectionManager& connections,
    std::vector<HSteamNetConnection> exceptions = {}
) {
    const std::array payload{std::byte{0x01}};
    const auto totalSize = sizeof(lightnerx::communication::Event) + payload.size();
    const lightnerx::communication::Event event{
        static_cast<std::uint16_t>(totalSize << 1),
        47
    };

    auto outbound = lightnerx::communication::event2OutboundMessage(
        event,
        payload,
        lightnerx::network::Target::broadcast(std::move(exceptions))
    );

    const std::span<const std::byte> bytes{
        static_cast<const std::byte*>(outbound.message->m_pData),
        static_cast<std::size_t>(outbound.message->m_cbSize)
    };

    const auto queued = lightnerx::network::queueMessage(bytes, std::move(outbound.target));
    outbound.message->Release();

    if (!queued) {
        return queued;
    }
    return lightnerx::network::flushAll(connections);
}
```

Chame `broadcastEvent(connections)` para todos ou `broadcastEvent(connections, {connectionToSkip})` para excluir um handle, verificando o `std::expected` retornado.

### O que cada etapa faz

`queueMessage` aloca uma mensagem interna e copia os bytes recebidos. O `std::move` aplicado a `outbound.target` transfere somente o destino. A chamada de `Release()` libera a mensagem original e seu buffer; a cópia enfileirada continua válida. Após liberar o original, não acesse `outbound.message` nem o span `bytes` construído sobre ele.

`flushAll` percorre as mensagens pendentes, encaminha cada uma conforme seu destino e libera sua cópia interna. Todas as entradas são processadas e a fila é esvaziada mesmo quando há erros de envio. O retorno informa sucesso ou o último erro encontrado entre essas operações.

O batching permite acumular várias mensagens antes de um flush. Cada entrada continua sendo uma mensagem separada; os payloads não são concatenados. A aplicação decide quando fazer o flush, por exemplo ao final de uma atualização. O sucesso indica que o transporte aceitou as operações de envio, sem confirmar o processamento no destinatário.

Para `Event` e `Response`, o campo `size` deve usar o valor combinado descrito acima. Para `Intention` e `Notification`, ele usa o tamanho total diretamente.
