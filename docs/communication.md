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
- `MBT::type`, usado como `type_id`;
- payload opcional.

### Response

`Response` relaciona o resultado a uma intenção por meio de `correlationId` e contém um `status`.

Os status são expostos pelas constantes do módulo `communication.response`:

| Constante | Valor | Significado | Tipo usado pelo LightnerX |
| --- | ---: | --- | --- |
| `APPROVED_RESPONSE` | `0` | intenção aprovada integralmente | `Response` |
| `DENIED_RESPONSE` | `1` | intenção negada | `UnsuccessfulResponse` |
| `PARTIALLY_APPROVED_RESPONSE` | `2` | intenção parcialmente aplicada | `UnsuccessfulResponse` |
| `FAILURE_RESPONSE` | `3` | falha no processamento | `Response` |

`UnsuccessfulResponse` acrescenta `reasonCode` ao cabeçalho. Para respostas parcialmente aprovadas, o payload contém a parte aplicada conforme o contrato da aplicação.

### Intention

`Intention` solicita que a Autoridade execute uma ação. Seu cabeçalho contém:

- `BaseProtocol::size`;
- `MBT::type`;
- `correlationId`;
- payload opcional.

### Notification

`Notification` comunica uma informação à Autoridade sem representar uma solicitação de alteração de estado. Seu cabeçalho contém `size`, `type` e payload opcional.

No servidor, o `NotificationRegistry` informa quais `type_id` devem ser interpretados como `Notification`:

```cpp
#include <cstdint>
#include <unordered_set>

communication::NotificationRegistry registry;
registry.registerId(47);
registry.registerIds(std::unordered_set<uint16_t>{99, 100});
```

Os valores de `type_id` são definidos pela aplicação. Não existe uma tabela universal de tipos INER.

## Representação binária

As estruturas são empacotadas sem padding adicional por meio de `#pragma pack(push, 1)`.

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

O interpretador recupera o tamanho com `base >> 1` e a direção da mensagem com `base & 1`.

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

Esse ID interno pertence à implementação de gerenciamento de conexões e não é um campo do protocolo INER.

## Fluxo de recebimento

```text
Rede -> callback do Server -> obedientInterpret -> callback da aplicação
Rede -> callback do Client -> authorityInterpret -> callback da aplicação
```

`MessageInterpreter` entrega um `communication::Message` cujo `header` contém o tipo concreto em uma `std::variant` e cujo `payload` é um `std::span<const std::byte>`.

## Fluxo de envio

O fluxo comum é:

1. Construir o cabeçalho com o tamanho total da mensagem.
2. Usar o conversor correspondente para obter um `network::OutboundMessage`.
3. Copiar seus bytes para `queueMessage`.
4. Liberar o `SteamNetworkingMessage_t` retornado pelo conversor.
5. Chamar `flushAll()` para enviar a fila.

Exemplo de `Notification`:

```cpp
const std::vector<std::byte> payload = /* dados da aplicação */;
const communication::Notification notification{
    static_cast<uint16_t>(sizeof(communication::Notification) + payload.size()),
    47
};

auto outbound = communication::notification2OutboundMessage(
    notification,
    {payload.data(), payload.size()},
    connection
);

const std::span<const std::byte> bytes{
    static_cast<const std::byte*>(outbound.message->m_pData),
    static_cast<std::size_t>(outbound.message->m_cbSize)
};

const auto queued = network::queueMessage(bytes, outbound.connection);
outbound.message->Release();

if (queued) {
    network::flushAll();
}
```

Para `Event` e `Response`, o campo `size` deve usar o valor combinado descrito acima. Para `Intention` e `Notification`, ele usa o tamanho total diretamente.
