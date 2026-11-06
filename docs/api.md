# Referência da API

Esta página apresenta os componentes públicos agrupados por módulo. Todos os módulos são importados com `import` e pertencem aos namespaces indicados.

## `communication.base_protocol`

### `communication::BaseProtocol`

Estrutura base dos cabeçalhos.

```cpp
BaseProtocol(std::uint16_t size);
```

- `size`: tamanho codificado da mensagem da Autoridade ou tamanho total da mensagem do Obediente, conforme o formato usado.
- Campo público: `std::uint16_t size`.

## `communication.mbt`

### `communication::MBT`

Armazena o identificador semântico da mensagem.

```cpp
MBT(std::uint16_t type);
```

- Campo público: `std::uint16_t type`.

## Tipos de mensagem

### `communication.event`

### `communication::Event`

Cabeçalho de um evento da Autoridade.

```cpp
Event(std::uint16_t size, std::uint16_t typeId);
```

Campos públicos:

- `protocol`: `BaseProtocol`;
- `mbt`: `MBT`.

### `communication.notification`

### `communication::Notification`

Cabeçalho de uma notificação do Obediente.

```cpp
Notification(std::uint16_t size, std::uint16_t type);
```

Campos públicos:

- `protocol`: `BaseProtocol`;
- `mbt`: `MBT`.

### `communication::NotificationRegistry`

Registra IDs que devem ser interpretados como `Notification` pelo servidor.

```cpp
void registerId(std::uint16_t id);
void registerIds(std::unordered_set<std::uint16_t>&& newIds);
bool exist(std::uint16_t id) const noexcept;
```

`exist()` informa se o ID está registrado.

### `communication.intention`

### `communication::Intention`

Cabeçalho de uma intenção enviada à Autoridade.

```cpp
Intention(
    std::uint16_t size,
    std::uint16_t type,
    std::uint16_t correlationId
);
```

Campos públicos:

- `protocol`: `BaseProtocol`;
- `mbt`: `MBT`;
- `correlationId`: identificador usado para relacionar a resposta.

### `communication.response`

### `communication::Response`

Cabeçalho usado para respostas aprovadas ou de falha.

```cpp
Response(
    std::uint16_t size,
    std::uint16_t correlationId,
    std::uint8_t status
);
```

Campos públicos:

- `protocol`: `BaseProtocol`;
- `correlationId`;
- `status`.

### `communication::UnsuccessfulResponse`

Cabeçalho usado para respostas negadas ou parcialmente aprovadas.

```cpp
UnsuccessfulResponse(
    std::uint16_t size,
    std::uint16_t correlationId,
    std::uint8_t status,
    std::uint16_t reasonCode
);
```

Campos públicos:

- `protocol`: `BaseProtocol`;
- `correlationId`;
- `status`;
- `reasonCode`.

### Constantes de status

```cpp
constexpr std::uint8_t APPROVED_RESPONSE = 0;
constexpr std::uint8_t DENIED_RESPONSE = 1;
constexpr std::uint8_t PARTIALLY_APPROVED_RESPONSE = 2;
constexpr std::uint8_t FAILURE_RESPONSE = 3;
```

## `communication.message`

### `communication::Message`

Resultado entregue pelo interpretador.

```cpp
struct Message {
    std::variant<
        Event,
        Intention,
        Response,
        UnsuccessfulResponse,
        Notification
    > header;
    std::span<const std::byte> payload;
};
```

Use `std::visit` para examinar o tipo armazenado em `header`.

### `communication::MessageInterpreter`

Interpreta bytes recebidos e chama callbacks registrados.

#### Registro de callbacks

```cpp
void addAuthorityInterpretedMessageCallback(
    std::function<void(Message)> callback
);

void addObedientInterpretedMessageCallback(
    std::function<void(Message, std::uint16_t)> callback
);
```

O callback da Autoridade recebe a mensagem interpretada. O callback do Obediente recebe a mensagem e o ID interno do cliente associado à conexão.

#### Interpretação no servidor

```cpp
void obedientInterpret(
    std::span<const std::byte> data,
    const NotificationRegistry& registry,
    HSteamNetConnection hConn,
    const network::ConnectionManager& connManager
);
```

Interpreta mensagens enviadas pelo Obediente. IDs encontrados no registro são tratados como `Notification`; os demais são tratados como `Intention`.

#### Interpretação no cliente

```cpp
void authorityInterpret(std::span<const std::byte> data);
```

Interpreta `Event` e respostas enviadas pela Autoridade.

### Conversores para saída

As funções abaixo retornam `network::OutboundMessage`:

```cpp
network::OutboundMessage event2OutboundMessage(
    const Event& event,
    std::span<const std::byte> payload,
    network::Target target
);

network::OutboundMessage intention2OutboundMessage(
    const Intention& intention,
    std::span<const std::byte> payload,
    network::Target target
);

network::OutboundMessage response2OutboundMessage(
    const Response& response,
    std::span<const std::byte> payload,
    network::Target target
);

network::OutboundMessage unsuccessfulResponse2OutboundMessage(
    const UnsuccessfulResponse& response,
    std::span<const std::byte> payload,
    network::Target target
);

network::OutboundMessage notification2OutboundMessage(
    const Notification& notification,
    std::span<const std::byte> payload,
    network::Target target
);
```

Os conversores alocam um `SteamNetworkingMessage_t`, copiam o cabeçalho e acrescentam o payload. O objeto retornado contém o ponteiro `message` e o `target` de destino.

## `network.network_manager`

Funções de controle do estado global da rede:

```cpp
bool networkInit();
void networkKill();
bool isNetworkInitialized();
```

`networkInit()` inicializa o GameNetworkingSockets e retorna `true` quando a rede está disponível.

## `network.server`

### `network::Server`

Servidor baseado em socket de escuta e grupo de polling.

#### Callbacks

```cpp
void addClientConnectedCallback(
    std::function<void(HSteamNetConnection)> callback
);

void addClientDisconnectedCallback(
    std::function<void(HSteamNetConnection)> callback
);

void addClientReceivedMessageCallback(
    std::function<void(
        std::span<const std::byte>,
        HSteamNetConnection
    )> callback
);
```

#### Ciclo de vida

```cpp
std::expected<void, std::error_code> start(
    int port,
    std::uint32_t timeoutConnected
);

std::expected<void, std::error_code> update();

std::expected<void, std::error_code> disconnectConnection(
    HSteamNetConnection hConn,
    bool enableLinger
);

void close();
```

`start()` abre o socket de escuta. `update()` processa callbacks da rede e uma mensagem disponível no grupo de polling. `disconnectConnection()` encerra uma conexão específica. `close()` encerra o socket de escuta e o grupo de polling.

## `network.client`

### `network::Client`

Cliente que conecta a um endereço IP e processa mensagens da conexão com o servidor.

```cpp
void addServerReceivedMessageCallback(
    std::function<void(std::span<const std::byte>)> callback
);

std::expected<void, std::error_code> start();

std::expected<void, std::error_code> connect(
    std::string_view ip,
    int port
);

std::expected<void, std::error_code> update(
    int maxMessagesToProcess
);

std::expected<void, std::error_code> disconnect(
    bool enableLinger
);

bool isConnected() const noexcept;
HSteamNetConnection getServerHConn() const noexcept;
```

`start()` inicializa o transporte. `connect()` inicia a conexão para o IP e a porta informados. `update()` executa os callbacks da rede e processa até o número indicado de mensagens. `getServerHConn()` retorna o handle usado para enviar mensagens ao servidor.

## `network.connection_manager`

### `network::ConnectionManager`

Mantém a associação entre IDs internos e handles de conexão.

```cpp
ConnectionManager(int initialReserve, int maxCapacity);

HSteamNetConnection getHSteamNetConnection(
    std::uint16_t id
) const noexcept;

std::optional<std::uint16_t> getClientId(
    HSteamNetConnection hConn
) const noexcept;

std::optional<std::uint16_t> add(HSteamNetConnection hConn);
void remove(int id);

int getOnlineCount() const noexcept;

void forEach(const auto& callback) const noexcept;
```

`forEach` chama o callback para cada conexão ativa, fornecendo o ID e o handle:

```cpp
connections.forEach(
    [](std::uint16_t id, HSteamNetConnection connection) {
        // Enviar ou examinar a conexão.
    }
);
```

## `network.transport`

### Envio direto

```cpp
std::expected<void, std::error_code> sendToConnection(
    std::span<const std::byte> message,
    HSteamNetConnection hConn
);
```

Envia bytes diretamente para uma conexão.

### Envio para grupo

```cpp
std::expected<void, std::error_code> sendToConnections(
    std::span<const std::byte> message,
    const std::vector<HSteamNetConnection> connections
);
```

Envia os mesmos bytes para cada handle presente no vetor.

### Broadcast

```cpp
std::expected<void, std::error_code> broadcast(
    std::span<const std::byte> message,
    const network::ConnectionManager& connectionManager,
    const std::vector<HSteamNetConnection> exceptions
);
```

Envia os mesmos bytes para as conexões ativas do `ConnectionManager`, exceto os handles presentes em `exceptions`.

## `network.target`

### `network::Target`

Representa o destino de uma mensagem enfileirada. Use as fábricas estáticas para criar o destino:

```cpp
static Target single(HSteamNetConnection conn);

static Target group(std::vector<HSteamNetConnection> connections);

static Target broadcast(
    std::vector<HSteamNetConnection> exceptions = {}
);

[[nodiscard]] bool isBroadcast() const noexcept;
[[nodiscard]] bool isSingle() const noexcept;
```

- `single` envia para uma conexão.
- `group` envia para os handles informados.
- `broadcast` envia para as conexões do `ConnectionManager` usado no flush; `exceptions` identifica handles que não receberão a mensagem.

Campos públicos:

```cpp
std::variant<
    std::monostate,
    std::vector<HSteamNetConnection>,
    HSteamNetConnection
> connection;

std::vector<HSteamNetConnection> broadcastExceptions;
```

## `network.message_batch`

### `network::OutboundMessage`

```cpp
struct OutboundMessage {
    SteamNetworkingMessage_t* message;
    Target target;
};
```

### Batcher

```cpp
void initMessagesBatcher(int capacity);

std::expected<void, std::error_code> queueMessage(
    std::span<const std::byte> message,
    Target target
);

std::expected<void, std::error_code> flushAll(
    const ConnectionManager& connectionManager
);
```

`initMessagesBatcher` reserva espaço para a fila. `queueMessage` copia os bytes e transfere o `Target` para uma mensagem gerenciada pelo batcher. `flushAll` envia as mensagens pendentes segundo seus destinos, libera as mensagens internas e esvazia a fila.

A mensagem apontada por `OutboundMessage::message` continua sob propriedade do chamador. Depois que `queueMessage` retorna, libere essa mensagem com `Release()`. O `std::move` aplicado a `OutboundMessage::target` transfere somente o destino para a fila.

O uso típico após um conversor é:

```cpp
auto outbound = communication::event2OutboundMessage(
    event,
    payload,
    network::Target::single(connection)
);

const std::span<const std::byte> bytes{
    static_cast<const std::byte*>(outbound.message->m_pData),
    static_cast<std::size_t>(outbound.message->m_cbSize)
};

const auto queued = network::queueMessage(
    bytes,
    std::move(outbound.target)
);
outbound.message->Release();

if (queued) {
    network::flushAll(connections);
}
```
