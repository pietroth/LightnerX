# Referência da API

Esta página apresenta os componentes públicos agrupados por módulo. Todos os módulos são importados com `import` e pertencem aos namespaces indicados.

As operações que retornam `std::expected<void, std::error_code>` podem ser verificadas com `if (!result)`. Em caso de erro, `result.error()` fornece o código e `result.error().message()` fornece sua descrição. Os exemplos completos estão em [Primeiros passos](first-steps.md).

## `communication.base_protocol`

### `communication::BaseProtocol`

Estrutura base dos cabeçalhos.

```cpp
BaseProtocol(std::uint16_t size);
```

- `size`: na construção para envio, tamanho codificado da mensagem da Autoridade ou tamanho total da mensagem do Obediente. O interpretador entrega esse campo com o tamanho já decodificado.
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

`registerId` adiciona um ID e `registerIds` incorpora os IDs de um conjunto passado como temporário ou com `std::move`. `exist()` informa se o ID está registrado. O registro é fornecido pela aplicação a `obedientInterpret`.

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

Use `std::visit(visitante, message.header)` para examinar o tipo armazenado em `header`. O cabeçalho é um valor; o `payload` referencia o buffer fornecido ao interpretador. Para usar o payload depois do callback de recebimento, copie os bytes para armazenamento da aplicação.

### `communication::MessageInterpreter`

Interpreta bytes recebidos e chama os callbacks registrados, na ordem de registro e durante a própria chamada de interpretação. É possível registrar mais de um callback. Lambdas que capturam objetos por referência precisam que esses objetos permaneçam vivos durante a execução dos callbacks.

#### Registro de callbacks

```cpp
void addAuthorityInterpretedMessageCallback(
    std::function<void(Message)> callback
);

void addObedientInterpretedMessageCallback(
    std::function<void(Message, std::uint16_t)> callback
);
```

O callback registrado com `addAuthorityInterpretedMessageCallback` recebe mensagens enviadas pela Autoridade, normalmente no cliente. O registrado com `addObedientInterpretedMessageCallback` recebe mensagens enviadas pelo Obediente, normalmente no servidor, acompanhadas do ID interno do cliente.

#### Interpretação no servidor

```cpp
void obedientInterpret(
    std::span<const std::byte> data,
    const NotificationRegistry& registry,
    HSteamNetConnection hConn,
    const network::ConnectionManager& connManager
);
```

Interpreta mensagens enviadas pelo Obediente. IDs encontrados no registro são tratados como `Notification`; os demais são tratados como `Intention`. `hConn` identifica a conexão de origem e `connManager` fornece sua associação com o ID interno. Mantenha essa associação atualizada ao conectar e desconectar clientes. A assinatura recebe especificamente `network::ConnectionManager`.

#### Interpretação no cliente

```cpp
void authorityInterpret(std::span<const std::byte> data);
```

Interpreta `Event` e respostas enviadas pela Autoridade. O bit de tipo seleciona evento ou resposta; o status seleciona `Response` ou `UnsuccessfulResponse`. Os status aprovado e falha entregam payload vazio; negativa e aprovação parcial entregam os bytes após `UnsuccessfulResponse`.

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

Inicialize a rede antes de usá-los. O chamador preenche o campo `size` conforme a [representação binária](communication.md#representação-binária) e é responsável por chamar `Release()` no objeto criado. Para respostas aprovadas, negadas ou de falha, forneça `{}` como payload; para aprovação parcial, forneça os bytes definidos pela aplicação.

## `network.network_manager`

Funções de controle do estado global da rede:

```cpp
bool networkInit();
void networkKill();
bool isNetworkInitialized();
```

`networkInit()` inicializa o GameNetworkingSockets e retorna `true` quando a inicialização é bem-sucedida ou já foi registrada. `isNetworkInitialized()` consulta esse estado. `networkKill()` encerra a rede e limpa o estado mantido por esse módulo. `Server::start()` e `Client::start()` já chamam `networkInit()`.

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

`start()` inicializa a rede e abre o socket de escuta na porta informada; `timeoutConnected` é passado ao GameNetworkingSockets em milissegundos. Após um início bem-sucedido, `update()` processa callbacks da rede e até uma mensagem disponível no grupo de polling.

O callback de conexão é chamado após a aceitação e associação ao grupo de polling. O de recebimento fornece os bytes e o handle da origem. Os bytes permanecem válidos durante esses callbacks.

`disconnectConnection()` solicita o encerramento de uma conexão específica e, em caso de sucesso, chama os callbacks de desconexão. `enableLinger` é repassado ao transporte para controlar a tentativa de envio dos dados pendentes ao fechar. `close()` fecha o socket de escuta, destrói o grupo de polling e encerra o GameNetworkingSockets.

## `network.client`

### `network::Client`

Cliente que conecta a um endereço IP e processa mensagens da conexão com o servidor.

```cpp
void addServerReceivedMessageCallback(
    const std::function<void(std::span<const std::byte>)>& callback
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

`start()` inicializa o transporte. `connect()` inicia a conexão para o endereço IP e a porta informados; acompanhe `isConnected()` enquanto chama `update()` para observar o estabelecimento da conexão. `update()` executa os callbacks da rede e processa até o número indicado de mensagens; valores menores ou iguais a zero executam somente os callbacks da rede.

`getServerHConn()` retorna o handle usado para enviar mensagens ao servidor, por exemplo com `Target::single(client.getServerHConn())`. O callback de recebimento é armazenado pelo cliente, e o span fornecido permanece válido durante sua execução.

`disconnect()` solicita o encerramento da conexão e repassa `enableLinger` ao transporte. O destrutor fecha a conexão existente e encerra o GameNetworkingSockets inicializado pelo cliente.

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

`initialReserve` reserva espaço para armazenamento e `maxCapacity` define a quantidade máxima de conexões registradas. `add` registra um handle e retorna o ID atribuído, ou um `std::optional` vazio quando o registro não é realizado. `remove` desfaz o registro do ID; o encerramento da conexão é feito pelo componente de rede.

`getHSteamNetConnection` consulta um handle pelo ID interno e retorna `0` quando não há handle nessa posição. `getClientId` consulta o ID associado ao handle no GameNetworkingSockets. `getOnlineCount` retorna a quantidade de registros ativos.

`forEach` chama o callback para cada registro ativo, fornecendo o ID e o handle:

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

Encaminha os bytes ao GameNetworkingSockets com a opção de envio confiável. O retorno indica se o transporte aceitou o envio. Essa função usa diretamente o transporte, sem passar pela fila do batcher.

### Envio para grupo

```cpp
std::expected<void, std::error_code> sendToConnections(
    std::span<const std::byte> message,
    const std::vector<HSteamNetConnection> connections
);
```

Encaminha os mesmos bytes para cada handle presente no vetor. Tenta todos os destinos e retorna o primeiro erro encontrado, quando houver.

### Broadcast

```cpp
std::expected<void, std::error_code> broadcast(
    std::span<const std::byte> message,
    const network::ConnectionManager& connectionManager,
    const std::vector<HSteamNetConnection> exceptions
);
```

Encaminha os mesmos bytes para as conexões registradas no `ConnectionManager`, exceto os handles presentes em `exceptions`. Tenta todos os destinos selecionados e retorna o primeiro erro encontrado, quando houver. No envio direto, o terceiro argumento é obrigatório; use `{}` para incluir todas as conexões:

```cpp
const auto all = network::broadcast(bytes, connections, {});
const auto others = network::broadcast(bytes, connections, {connectionToSkip});
```

Os exemplos acima são chamadas independentes; verifique o resultado de cada envio utilizado pela aplicação. Para adiar o envio até um flush, use `Target::broadcast` com `queueMessage`.

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

- `single` seleciona uma conexão.
- `group` guarda o vetor de handles informado.
- `broadcast()` seleciona todas as conexões registradas no `ConnectionManager` usado no flush. `broadcast({handle})` exclui o handle informado; o vetor pode conter várias exceções.

As fábricas apenas descrevem destinos. `isSingle()` informa se o destino é uma conexão individual; `isBroadcast()` informa se é um broadcast. O broadcast consulta as conexões no momento do flush, enquanto o grupo usa a lista armazenada no `Target`.

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

`initMessagesBatcher` reserva espaço na fila compartilhada do batcher. Chame-o antes de enfileirar, com a quantidade de mensagens que deseja poder manter pendentes; essa reserva não esvazia uma fila existente.

`queueMessage` copia os bytes e armazena o destino. Retorna `std::errc::not_enough_memory` quando a capacidade reservada está ocupada ou a alocação da mensagem interna falha. O parâmetro `Target` é recebido por valor: passar um destino existente sem `std::move` o copia; usar `std::move` permite transferir seus vetores.

`flushAll` processa cada mensagem separadamente com envio individual, para grupo ou broadcast. O `ConnectionManager` é um argumento obrigatório e fornece os destinatários dos broadcasts. A função libera todas as mensagens internas e esvazia a fila mesmo quando há erros, retornando o último erro entre as operações de envio. Uma fila vazia retorna sucesso.

A mensagem apontada por `OutboundMessage::message` continua sob propriedade do chamador. Depois que `queueMessage` retorna, libere essa mensagem uma única vez com `Release()`, inclusive quando o retorno indica erro. Isso libera o original e seu buffer; a cópia do batcher permanece válida. `std::move(outbound.target)` transfere somente o destino e não altera o ponteiro da mensagem original. Depois de `Release()`, não acesse esse ponteiro nem o span construído sobre ele.

Com a rede e o batcher inicializados, e `event`, `payload`, `connection` e `connections` definidos pela aplicação, o uso típico após um conversor é:

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

if (!queued) {
    std::println("Falha ao enfileirar: {}", queued.error().message());
} else if (const auto flushed = network::flushAll(connections); !flushed) {
    std::println("Falha ao enviar: {}", flushed.error().message());
}
```

Também é possível enfileirar várias mensagens antes de chamar `flushAll`. O batcher mantém cada mensagem separada e o momento do flush é escolhido pela aplicação.
