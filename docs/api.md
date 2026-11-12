# API reference

This page presents the public components grouped by module. All modules are imported with `import` and belong to the indicated namespaces.

Operations that return `std::expected<void, std::error_code>` can be checked with `if (!result)`. In the event of an error, `result.error()` provides the code and `result.error().message()` provides its description. Complete examples are available in [First steps](first-steps.md).

## `lightnerx.communication.base_protocol`

### `lightnerx::communication::BaseProtocol`

Base structure for headers.

```cpp
BaseProtocol(std::uint16_t size);
```

- `size`: when constructing a message for sending, the encoded size of the Authority's message or the total size of the Obedient's message. The interpreter delivers this field with the size already decoded.
- Public field: `std::uint16_t size`.

## `lightnerx.communication.mbt`

### `lightnerx::communication::MBT`

Stores the message's semantic identifier.

```cpp
MBT(std::uint16_t type);
```

- Public field: `std::uint16_t type`.

## Message types

### `lightnerx.communication.event`

### `lightnerx::communication::Event`

Header for an event from the Authority.

```cpp
Event(std::uint16_t size, std::uint16_t typeId);
```

Public fields:

- `protocol`: `BaseProtocol`;
- `mbt`: `MBT`.

### `lightnerx.communication.notification`

### `lightnerx::communication::Notification`

Header for a notification from the Obedient.

```cpp
Notification(std::uint16_t size, std::uint16_t type);
```

Public fields:

- `protocol`: `BaseProtocol`;
- `mbt`: `MBT`.

### `lightnerx::communication::NotificationRegistry`

Registers IDs that the server should interpret as `Notification`.

```cpp
void registerId(std::uint16_t id);
void registerIds(std::unordered_set<std::uint16_t>&& newIds);
bool exist(std::uint16_t id) const noexcept;
```

`registerId` adds an ID, and `registerIds` incorporates the IDs from a set passed as a temporary or with `std::move`. `exist()` reports whether the ID is registered. The application provides the registry to `obedientInterpret`.

### `lightnerx.communication.intention`

### `lightnerx::communication::Intention`

Header for an intention sent to the Authority.

```cpp
Intention(
    std::uint16_t size,
    std::uint16_t type,
    std::uint16_t correlationId
);
```

Public fields:

- `protocol`: `BaseProtocol`;
- `mbt`: `MBT`;
- `correlationId`: identifier used to correlate the response.

### `lightnerx.communication.response`

### `lightnerx::communication::Response`

Header used for approved or failure responses.

```cpp
Response(
    std::uint16_t size,
    std::uint16_t correlationId,
    std::uint8_t status
);
```

Public fields:

- `protocol`: `BaseProtocol`;
- `correlationId`;
- `status`.

### `lightnerx::communication::UnsuccessfulResponse`

Header used for denied or partially approved responses.

```cpp
UnsuccessfulResponse(
    std::uint16_t size,
    std::uint16_t correlationId,
    std::uint8_t status,
    std::uint16_t reasonCode
);
```

Public fields:

- `protocol`: `BaseProtocol`;
- `correlationId`;
- `status`;
- `reasonCode`.

### Status constants

```cpp
constexpr std::uint8_t APPROVED_RESPONSE = 0;
constexpr std::uint8_t DENIED_RESPONSE = 1;
constexpr std::uint8_t PARTIALLY_APPROVED_RESPONSE = 2;
constexpr std::uint8_t FAILURE_RESPONSE = 3;
```

## `lightnerx.communication.message`

### `lightnerx::communication::Message`

Result delivered by the interpreter.

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

Use `std::visit(visitor, message.header)` to examine the type stored in `header`. The header is a value; `payload` references the buffer provided to the interpreter. To use the payload after the receive callback, copy the bytes into application storage.

### `lightnerx::communication::MessageInterpreter`

Interprets received bytes and calls registered callbacks in registration order, during the interpretation call itself. More than one callback can be registered. Lambdas that capture objects by reference require those objects to remain alive while the callbacks run.

#### Callback registration

```cpp
void addAuthorityInterpretedMessageCallback(
    std::function<void(Message)> callback
);

void addObedientInterpretedMessageCallback(
    std::function<void(Message, std::uint16_t)> callback
);
```

The callback registered with `addAuthorityInterpretedMessageCallback` receives messages sent by the Authority, typically on the client. The one registered with `addObedientInterpretedMessageCallback` receives messages sent by the Obedient, typically on the server, along with the internal client ID.

#### Interpretation on the server

```cpp
void obedientInterpret(
    std::span<const std::byte> data,
    const NotificationRegistry& registry,
    HSteamNetConnection hConn,
    const lightnerx::network::ConnectionManager& connManager
);
```

Interprets messages sent by the Obedient. IDs found in the registry are treated as `Notification`; the others are treated as `Intention`. `hConn` identifies the source connection, and `connManager` provides its mapping to the internal ID. Keep this mapping up to date when connecting and disconnecting clients. The signature specifically takes `lightnerx::network::ConnectionManager`.

#### Interpretation on the client

```cpp
void authorityInterpret(std::span<const std::byte> data);
```

Interprets `Event` and responses sent by the Authority. The type bit selects event or response; the status selects `Response` or `UnsuccessfulResponse`. The approved and failure statuses deliver an empty payload; denial and partial approval deliver the bytes following `UnsuccessfulResponse`.

This reception behavior is more permissive than the recommended emission formats. If the total size matches the supplied span and the header is complete, additional bytes in approved and failure responses are accepted but ignored; additional bytes in denied responses are delivered. The decoded `protocol.size` includes any ignored bytes, so it does not necessarily equal the header size plus the delivered payload size. See [binary representation](communication.md#binary-representation) for both emission and reception rules.

### Outbound converters

The functions below return `lightnerx::network::OutboundMessage`:

```cpp
lightnerx::network::OutboundMessage event2OutboundMessage(
    const Event& event,
    std::span<const std::byte> payload,
    lightnerx::network::Target target
);

lightnerx::network::OutboundMessage intention2OutboundMessage(
    const Intention& intention,
    std::span<const std::byte> payload,
    lightnerx::network::Target target
);

lightnerx::network::OutboundMessage response2OutboundMessage(
    const Response& response,
    std::span<const std::byte> payload,
    lightnerx::network::Target target
);

lightnerx::network::OutboundMessage unsuccessfulResponse2OutboundMessage(
    const UnsuccessfulResponse& response,
    std::span<const std::byte> payload,
    lightnerx::network::Target target
);

lightnerx::network::OutboundMessage notification2OutboundMessage(
    const Notification& notification,
    std::span<const std::byte> payload,
    lightnerx::network::Target target
);
```

The converters allocate a `SteamNetworkingMessage_t`, copy the header, and append the payload. The returned object contains the `message` pointer and the destination `target`. Allocation failure throws `std::bad_alloc`; a byte count that cannot fit the GNS allocation API throws `std::length_error`. No message ownership is transferred on these exceptions.

Initialize the network before using them. The caller fills in the `size` field according to the [binary representation](communication.md#binary-representation) and passes the created object to `queueMessage`, which takes ownership of it. If the object is never passed to the queue, the caller must release it with `Release()`. For the recommended emission formats, provide `{}` as the payload for approved, denied, or failure responses; for partial approval, provide the bytes defined by the application. Converters copy the supplied payload without enforcing these status-specific recommendations.

## `lightnerx.network.network_manager`

Functions for controlling the global network state:

```cpp
bool networkInit();
void networkKill();
bool isNetworkInitialized();
```

`networkInit()` initializes GameNetworkingSockets and is idempotent. `isNetworkInitialized()` reports whether the registered runtime and interface exist. Without a started wrapper, `networkKill()` releases queued messages, shuts down networking and resets the state; it is safe to call repeatedly and initialize again.

Exactly one started `Client` **or** `Server` may own the runtime per process. Starting the same owner again returns `operation_in_progress`; another owner returns `device_or_resource_busy`. Both types are noncopyable and nonmovable. A failed start cleans up acquired resources and permits retry. `Server::close()` (also called by its destructor) or the client destructor releases ownership and destroys the runtime, including a runtime initialized manually before `start()`.

While a wrapper owns the runtime, `networkKill()` defers shutdown until that owner releases it. The interface remains usable and `isNetworkInitialized()` remains true in the meantime. Direct external `GameNetworkingSockets_Kill()` and raw `RunCallbacks()` during wrapper ownership are unsupported; use LightnerX lifecycle and update operations. Synchronize all network operations across threads in the application.

`k_invalidConnection` has one declaration in this module, re-exported by the client and transport modules.

## `lightnerx.network.server`

### `lightnerx::network::Server`

Server based on a listen socket and a poll group.

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

#### Lifecycle

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

void close() noexcept;
```

`start()` initializes the network and opens the listen socket on the specified port (1–65535); `timeoutConnected` is passed to GameNetworkingSockets in milliseconds. Its maximum accepted value is `INT32_MAX` (2,147,483,647 ms), although the parameter type is `uint32_t`. For an unstarted server, a value above this limit returns `std::errc::invalid_argument` before initializing the network or opening the listen socket. Calling `start()` on an already started server returns `operation_in_progress` first. After a successful start, `update()` processes network callbacks and up to one available message in the poll group.

The connection callback is called after acceptance and assignment to the poll group. The receive callback provides the bytes and the source handle. The bytes remain valid during these callbacks.

`disconnectConnection()` requests that a specific connection be closed and, on success, calls the disconnection callbacks. `enableLinger` is forwarded to the transport to control the attempt to send pending data when closing. `close()` closes the listen socket, destroys the poll group, and shuts down GameNetworkingSockets. It is idempotent and also runs from the destructor. A close requested inside a callback is completed after the outer operation releases received messages; remaining callbacks in that dispatch are skipped. Bulk close does not emit individual disconnection callbacks: clear application registrations before reusing a manager for a new session. Disconnection callbacks receive a historical handle after closure; `ConnectionManager::getClientId()` remains valid until `remove()`.

## `lightnerx.network.client`

### `lightnerx::network::Client`

Client that connects to an IP address and processes messages from the connection to the server.

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

`start()` initializes the transport. `connect()` initiates the connection to the specified IP address and port; monitor `isConnected()` while calling `update()` to observe the connection being established. `update()` runs network callbacks and processes up to the specified number of messages; values less than or equal to zero run only the network callbacks.

`getServerHConn()` returns the handle used to send messages to the server, for example with `Target::single(client.getServerHConn())`. The receive callback is stored by the client, and the provided span remains valid during its execution.

`disconnect()` requests that the connection be closed and forwards `enableLinger` to the transport. On success it immediately invalidates the handle and clears `isConnected()`, allowing another `connect()` on the same instance. It retains ownership of the runtime until the destructor. Disconnecting inside a receive callback stops further callbacks and messages from that receive batch; all acquired messages are released. The destructor closes the existing connection and shuts down networking.

## `lightnerx.network.connection_manager`

### `lightnerx::network::ConnectionManager`

Maintains the mapping between internal IDs and connection handles.

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

void forEach(const auto& callback) const;
```

`initialReserve` reserves storage space, and `maxCapacity` defines the maximum number of registered connections. `add` registers a handle and returns the assigned ID, or an empty `std::optional` when registration is not performed. `remove` unregisters the ID; closing the connection is handled by the networking component.

Copying a manager produces independent registrations. Copy assignment provides the strong exception guarantee: if copying throws, the destination's mappings, count, capacity and ID allocation state remain unchanged.

`getHSteamNetConnection` looks up a handle by its internal ID and returns `0` when there is no handle at that position. `getClientId` looks up the ID in this manager's local registration map, independently of GNS userdata or whether the handle has since closed. `getOnlineCount` returns the number of active registrations.

`forEach` calls the callback for each active registration, providing the ID and handle:

```cpp
connections.forEach(
    [](std::uint16_t id, HSteamNetConnection connection) {
        // Send to or inspect the connection.
    }
);
```

## `lightnerx.network.transport`

### Direct sending

```cpp
std::expected<void, std::error_code> sendToConnection(
    std::span<const std::byte> message,
    HSteamNetConnection hConn
);
```

Forwards the bytes to GameNetworkingSockets with the reliable send option. The return value indicates whether the transport accepted the send. This function uses the transport directly, bypassing the batcher queue.

### Sending to a group

```cpp
std::expected<void, std::error_code> sendToConnections(
    std::span<const std::byte> message,
    const std::vector<HSteamNetConnection> connections
);
```

Forwards the same bytes to each handle in the vector. Attempts all destinations and returns the first error encountered, if any.

### Broadcast

```cpp
std::expected<void, std::error_code> broadcast(
    std::span<const std::byte> message,
    const lightnerx::network::ConnectionManager& connectionManager,
    const std::vector<HSteamNetConnection> exceptions
);
```

Forwards the same bytes to the connections registered in `ConnectionManager`, except for the handles in `exceptions`. Attempts all selected destinations and returns the first error encountered, if any. For direct sending, the third argument is required; use `{}` to include all connections:

```cpp
const auto all = lightnerx::network::broadcast(bytes, connections, {});
const auto others = lightnerx::network::broadcast(bytes, connections, {connectionToSkip});
```

The examples above are independent calls; check the result of each send used by the application. To defer sending until a flush, use `Target::broadcast` with `queueMessage`.

## `lightnerx.network.target`

### `lightnerx::network::Target`

Represents the destination of a queued message. Use the static factories to create the destination:

```cpp
static Target single(HSteamNetConnection conn);

static Target group(std::vector<HSteamNetConnection> connections);

static Target broadcast(
    std::vector<HSteamNetConnection> exceptions = {}
);

[[nodiscard]] bool isBroadcast() const noexcept;
[[nodiscard]] bool isSingle() const noexcept;
[[nodiscard]] bool isGroup() const noexcept;
```

- `single` selects one connection.
- `group` stores the provided vector of handles.
- `broadcast()` selects all connections registered in the `ConnectionManager` used during the flush. `broadcast({handle})` excludes the specified handle; the vector can contain multiple exceptions.

The factories only describe destinations. `isSingle()`, `isGroup()` and `isBroadcast()` report which destination is stored. Broadcast queries connections at flush time, while a group uses the list stored in `Target`.

Public fields:

```cpp
std::variant<
    std::monostate,
    std::vector<HSteamNetConnection>,
    HSteamNetConnection
> connection;

std::vector<HSteamNetConnection> broadcastExceptions;
```

## `lightnerx.network.message_batch`

### `lightnerx::network::OutboundMessage`

```cpp
struct OutboundMessage {
    SteamNetworkingMessage_t* message;
    Target target;
};
```

### Batcher

```cpp
void initMessagesBatcher(int capacity);
void clearMessagesBatcher() noexcept;

std::expected<void, std::error_code> queueMessage(OutboundMessage message);

std::expected<void, std::error_code> flushAll(
    const ConnectionManager& connectionManager
);
```

`initMessagesBatcher` reserves space in the batcher's shared queue. Call it before queuing, with the number of messages you want to be able to keep pending; this reservation does not empty an existing queue.

`queueMessage` receives an `OutboundMessage` by value and moves it into the queue. The message allocated by the converter is retained, without allocating another message or copying the bytes. If the reserved capacity is occupied, it releases the received message and returns `std::errc::not_enough_memory`. If `message.message` is null, it returns `std::errc::invalid_argument` without queuing.

`flushAll` processes each message separately by sending it to an individual connection, a group, or a broadcast. `ConnectionManager` is a required argument and provides the broadcast recipients. The function releases all internal messages and empties the queue even when errors occur, returning the last error among the send operations. An empty queue returns success.

Calling `queueMessage` consumes the message, both on success and on error. Pass an existing object with `std::move(outbound)` or use a converter's result directly. Afterward, do not access, release, or requeue the message. `OutboundMessage` contains a raw pointer: `std::move` allows the destination to be moved, but does not automatically set the pointer to null in the original object. Copying the object does not duplicate the message either; therefore, do not pass copies of the same message to the queue. If you never pass a created message to the queue, release it exactly once with `Release()`.

With the network and batcher initialized, and `event`, `payload`, `connection`, and `connections` defined by the application, typical usage after a converter is:

```cpp
auto outbound = lightnerx::communication::event2OutboundMessage(
    event,
    payload,
    lightnerx::network::Target::single(connection)
);

const auto queued = lightnerx::network::queueMessage(std::move(outbound));

if (!queued) {
    std::println("Failed to queue: {}", queued.error().message());
} else if (const auto flushed = lightnerx::network::flushAll(connections); !flushed) {
    std::println("Failed to send: {}", flushed.error().message());
}
```

You can also queue multiple messages before calling `flushAll`. The batcher keeps each message separate, and the application chooses when to flush.

## Validation and callback behavior

Both interpretation methods accept exactly one complete message. The declared total size must equal the supplied span and include the entire header. Authority sizes are decoded from 15 bits (maximum 32767 bytes); Obedient sizes use 16 bits (maximum 65535 bytes). Truncated headers, mismatched sizes, unknown response status and unregistered Obedient origins produce no callback, matching the existing void parser interface. There is no incremental or concatenated-message framing.

Callbacks run synchronously. Registration during a callback is safe; newly registered callbacks participate from the next dispatch, including a nested interpretation. Existing callback objects retain mutable state. Receive/interpreter/`forEach` exceptions propagate to their caller, with received buffers released even when a client batch is interrupted. Exceptions from GNS status callbacks are caught at the boundary and the first exception is rethrown by `Server::update()` after `RunCallbacks()` returns. A throwing callback ends its current callback sequence.

Recursive `update()` returns `operation_in_progress`. Callbacks may disconnect or request `Server::close()`; do not destroy or assign the component executing a callback. Referenced objects must remain alive until dispatch finishes. A negative receive result returns `io_error`, distinct from an empty queue; the client also clears and closes its failed connection. Updating an unstarted/closed wrapper returns `not_connected`.

`ConnectionManager` owns an independent, bidirectional mapping; it neither reads nor writes GNS userdata. A handle may be registered once per manager. Adding a duplicate, invalid handle, or a handle beyond capacity returns `nullopt`. Different managers may independently register the same valid handle. `getClientId()` returns `nullopt` for an absent registration, including handle zero, and retains a registered identity after transport closure until removal. IDs range from 1 to 65535 and are recycled; negative capacities throw `invalid_argument`, capacity above 65535 is capped. `forEach` visits the index range present on entry, safely allowing add/remove; it may observe replacements within that range but does not extend traversal for vector growth.

The batch queue is global. `initMessagesBatcher()` reserves capacity without discarding queued messages. `clearMessagesBatcher()` releases pending messages and preserves capacity; actual network shutdown and queue destruction also release pending messages. `flushAll()` releases every queued message if a send throws, then propagates the exception; with returned error codes it attempts all entries and returns the last error. Serialize all batch operations, including clearing and shutdown.
