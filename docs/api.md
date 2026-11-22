# API reference

This page groups public components by header. Include the indicated header with `#include`; declarations belong to the indicated namespaces. Signature blocks are excerpts from class or namespace declarations, while usage blocks assume the objects described in their surrounding text.

The library uses `std::string_view` for non-owning views of binary buffers and `lightnerx::Result` for values or errors. See [C++17 byte views and results](cpp17-types.md) for the headers and examples.

Operations that return `lightnerx::Result<void>` can be checked with `if (!result)`. In the event of an error, `result.error()` provides the code and `lightnerx::errorMessage(result.error())` provides its description. Complete examples are available in [First steps](first-steps.md).

## `<lightnerx/communication/base_protocol.hpp>`

### `lightnerx::communication::BaseProtocol`

Base structure for headers.

```cpp
BaseProtocol(std::uint16_t size);
```

- `size`: when constructing a message for sending, the encoded size of the Authority's message or the total size of the Obedient's message. The interpreter delivers this field with the size already decoded.
- Public field: `std::uint16_t size`.

## `<lightnerx/communication/mbt.hpp>`

### `lightnerx::communication::MBT`

Stores the message's semantic identifier.

```cpp
MBT(std::uint16_t type);
```

- Public field: `std::uint16_t type`.

## Message types

### `<lightnerx/communication/event.hpp>`

### `lightnerx::communication::Event`

Header for an event from the Authority.

```cpp
Event(std::uint16_t size, std::uint16_t typeId);
```

Public fields:

- `protocol`: `BaseProtocol`;
- `mbt`: `MBT`.

### `<lightnerx/communication/notification.hpp>`

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

### `<lightnerx/communication/intention.hpp>`

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

### `<lightnerx/communication/response.hpp>`

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
inline constexpr std::uint8_t APPROVED_RESPONSE = 0;
inline constexpr std::uint8_t DENIED_RESPONSE = 1;
inline constexpr std::uint8_t PARTIALLY_APPROVED_RESPONSE = 2;
inline constexpr std::uint8_t FAILURE_RESPONSE = 3;
```

## `<lightnerx/communication/message.hpp>`

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
    std::string_view payload;
};
```

Use `std::visit(visitor, message.header)` to examine the type stored in `header`. The header is a value. The `payload` view borrows the buffer passed to the interpreter. To use the payload after a network receive callback, copy the bytes into application storage before returning.

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
    std::string_view data,
    const NotificationRegistry& registry,
    lightnerx::network::ConnectionHandle hConn,
    const lightnerx::network::ConnectionManager& connManager
);
```

Interprets messages sent by the Obedient. IDs found in the registry are treated as `Notification`; the others are treated as `Intention`. `hConn` identifies the source connection, and `connManager` provides its mapping to the internal ID. Keep this mapping up to date when connecting and disconnecting clients. The signature specifically takes `lightnerx::network::ConnectionManager`.

#### Interpretation on the client

```cpp
void authorityInterpret(std::string_view data);
```

Interprets `Event` and responses sent by the Authority. The type bit selects event or response; the status selects `Response` or `UnsuccessfulResponse`. The approved and failure statuses deliver an empty payload; denial and partial approval deliver the bytes following `UnsuccessfulResponse`.

This reception behavior is more permissive than the recommended emission formats. If the total size matches the supplied view and the header is complete, additional bytes in approved and failure responses are accepted but ignored; additional bytes in denied responses are delivered. The decoded `protocol.size` includes any ignored bytes, so it does not necessarily equal the header size plus the delivered payload size. See [binary representation](communication.md#binary-representation) for both emission and reception rules.

### Outbound converters

The functions below return `lightnerx::Result<lightnerx::network::OutboundMessage>`.

```cpp
lightnerx::Result<lightnerx::network::OutboundMessage> event2OutboundMessage(
    const Event& event,
    std::string_view payload,
    lightnerx::network::Target target
);

lightnerx::Result<lightnerx::network::OutboundMessage> intention2OutboundMessage(
    const Intention& intention,
    std::string_view payload,
    lightnerx::network::Target target
);

lightnerx::Result<lightnerx::network::OutboundMessage> response2OutboundMessage(
    const Response& response,
    std::string_view payload,
    lightnerx::network::Target target
);

lightnerx::Result<lightnerx::network::OutboundMessage> unsuccessfulResponse2OutboundMessage(
    const UnsuccessfulResponse& response,
    std::string_view payload,
    lightnerx::network::Target target
);

lightnerx::Result<lightnerx::network::OutboundMessage> notification2OutboundMessage(
    const Notification& notification,
    std::string_view payload,
    lightnerx::network::Target target
);
```

The converters copy the header and payload into an owning `OutboundMessage` and store `target` as the destination. They return `ErrorCode::NotEnoughMemory` for a failed transport allocation, `ErrorCode::MessageSize` if the combined byte count exceeds `INT_MAX` (the allocator's size limit), or `ErrorCode::IoError` if the transport utilities are unavailable.

Initialize the network before using them, then pass the result to `queueMessage`. The caller supplies the encoded size and checks the INER frame limit. The converters copy the header as supplied. In particular, an interpreted Authority header must be rebuilt with the encoded size before sending it again. Follow the recommended payload formats and the example for [forwarding a received event](communication.md#forwarding-a-received-event). See the [INER binary representation](communication.md#binary-representation).

## `<lightnerx/network/network_manager.hpp>`

Functions for controlling the global network state:

```cpp
bool networkInit();
void networkKill();
bool isNetworkInitialized();
```

`networkInit()` initializes GameNetworkingSockets and returns `true` on success, including when the network is already initialized. It returns `false` if initialization fails. `isNetworkInitialized()` reports whether the registered runtime and interface exist. Without a started wrapper, `networkKill()` releases queued messages, shuts down networking and resets the state. It is safe to call repeatedly and initialize again.

A single LightnerX library instance allows exactly one started `Client` or `Server` to own the runtime. Starting the same owner again returns `ErrorCode::OperationInProgress`. A valid start request from another owner returns `ErrorCode::DeviceOrResourceBusy`. Both wrappers return `ErrorCode::IoError` if the transport cannot initialize or supply its interface. Both types are noncopyable and nonmovable. A failed start releases resources created by that attempt and permits retry. If the application initialized the network manually, the failed attempt preserves the runtime, existing connections and pending messages. A successful start adopts that runtime. The server close or the client destructor releases ownership and shuts down the adopted runtime. For multiple plugins, use one shared LightnerX library across the process. See [consuming with CMake](setup.md#consuming-with-cmake).

While a wrapper owns the runtime, `networkKill()` defers shutdown until that owner releases it. The interface remains usable and `isNetworkInitialized()` remains true in the meantime. Direct external `GameNetworkingSockets_Kill()` and raw `RunCallbacks()` during wrapper ownership are unsupported; use LightnerX lifecycle and update operations. Synchronize all network operations across threads in the application.

## `<lightnerx/network/connection_handle.hpp>`

`lightnerx::network::ConnectionHandle` is an alias for `std::uint32_t`. The same header defines `lightnerx::network::k_invalidConnection` as zero. Use the constant to check whether a handle is absent. These transport handles are distinct from the client IDs assigned by `ConnectionManager`.

## `<lightnerx/network/server.hpp>`

### `lightnerx::network::Server`

Server based on a listen socket and a poll group.

#### Callbacks

```cpp
void addClientConnectedCallback(
    std::function<void(lightnerx::network::ConnectionHandle)> callback
);

void addClientDisconnectedCallback(
    std::function<void(lightnerx::network::ConnectionHandle)> callback
);

void addClientReceivedMessageCallback(
    std::function<void(
        std::string_view,
        lightnerx::network::ConnectionHandle
    )> callback
);
```

#### Lifecycle

```cpp
lightnerx::Result<void> start(
    int port,
    std::uint32_t timeoutConnected
);

lightnerx::Result<void> update();

lightnerx::Result<void> disconnectConnection(
    lightnerx::network::ConnectionHandle hConn,
    bool enableLinger
);

void close() noexcept;
```

`start()` initializes the network and opens the listen socket on the specified port (1–65535). `timeoutConnected` configures the transport timeout for established connections, in milliseconds, and accepts values from zero to `INT32_MAX` (2,147,483,647 ms). An out-of-range port or timeout returns `ErrorCode::InvalidArgument` before initialization. A start on an already started server returns `ErrorCode::OperationInProgress` before checking the arguments. Failure to create the listen socket or poll group returns `ErrorCode::ResourceUnavailableTryAgain`. After a successful start, `update()` processes network callbacks and up to one available message in the poll group. An empty receive queue is a successful update.

The connection callback runs after acceptance and assignment to the poll group. If a callback disconnects that client, disconnection callbacks run as part of the disconnect call and subsequent connection callbacks for that handle are skipped. The receive callback provides the bytes and the source handle. The bytes remain valid during these callbacks.

`disconnectConnection()` requests that a specific connection be closed and, on success, calls the disconnection callbacks. `enableLinger` is forwarded to the transport to control the attempt to send pending data when closing. An unstarted server or a pending `close()` returns `ErrorCode::NotConnected`. A close rejected by the transport returns `ErrorCode::IoError`.

`close()` closes the listen socket, destroys the poll group, and shuts down GameNetworkingSockets. It is idempotent and also runs from the destructor. A close requested inside a callback is completed after the outer operation releases received messages. Remaining callbacks in that dispatch are skipped. Bulk close bypasses individual disconnection callbacks and leaves application registrations in place. Clear them before reusing a manager for a new session. Disconnection callbacks receive a historical handle after closure. `ConnectionManager::getClientId()` remains valid until `remove()`.

## `<lightnerx/network/client.hpp>`

### `lightnerx::network::Client`

Client that connects to an IP address and processes messages from the connection to the server.

```cpp
void addServerReceivedMessageCallback(
    const std::function<void(std::string_view)>& callback
);

lightnerx::Result<void> start();

lightnerx::Result<void> connect(
    std::string_view ip,
    int port
);

lightnerx::Result<void> update(
    int maxMessagesToProcess
);

lightnerx::Result<void> disconnect(
    bool enableLinger
);

bool isConnected() const noexcept;
lightnerx::network::ConnectionHandle getServerHConn() const noexcept;
```

`start()` initializes the transport. `connect(ip, port)` initiates an asynchronous connection to a numeric IPv4 or IPv6 literal and a separate port in the range 1–65535. IPv6 may be enclosed in brackets. An invalid port, empty address, hostname, scope identifier, embedded port, embedded NUL or trailing character returns `ErrorCode::InvalidArgument`. Calling `connect()` before `start()` returns `ErrorCode::NotConnected`. An existing or pending connection returns `ErrorCode::AlreadyConnected`. The transport's failure to initiate the connection returns `ErrorCode::IoError`. Constructing the address string can propagate `std::bad_alloc` or `std::length_error`.

`update(maxMessagesToProcess)` runs network callbacks and processes up to that many messages. Values less than or equal to zero run only the network callbacks. With a pending or established handle and a positive limit, the receive step allocates message slots up to that limit and can propagate an allocation or container size exception. An initialized client with no handle can update successfully.

After `connect()` succeeds, call `update()` and monitor `isConnected()` for establishment. `getServerHConn()` returns a handle as soon as the attempt starts, or `k_invalidConnection` if no connection is pending or active. For sending, wait for `isConnected()` and use the handle, for example with `Target::single(client.getServerHConn())`. When `update()` processes a closure from the peer or a locally detected transport problem, it clears `isConnected()` and invalidates the handle. The update can still succeed, so check the handle to detect that the attempt or session has ended. The receive callback is stored by the client, and the provided view remains valid during its execution.

`disconnect()` requests that the connection be closed and forwards `enableLinger` to the transport. An unstarted client or missing handle returns `ErrorCode::NotConnected`. A close rejected by the transport returns `ErrorCode::IoError`. On success, the client immediately invalidates the handle and clears `isConnected()`, allowing another `connect()` on the same instance. It retains ownership of the runtime until the destructor. Disconnecting inside a receive callback stops further callbacks and messages from that receive batch. All acquired messages are released. The destructor closes the existing connection and shuts down networking.

## `<lightnerx/network/connection_manager.hpp>`

### `lightnerx::network::ConnectionManager`

Maintains the mapping between internal IDs and connection handles.

```cpp
ConnectionManager(int initialReserve, int maxCapacity);

lightnerx::network::ConnectionHandle getHSteamNetConnection(
    std::uint16_t id
) const noexcept;

std::optional<std::uint16_t> getClientId(
    lightnerx::network::ConnectionHandle hConn
) const noexcept;

std::optional<std::uint16_t> add(lightnerx::network::ConnectionHandle hConn);
void remove(int id);

int getOnlineCount() const noexcept;

template<class Callback>
void forEach(const Callback& callback) const;
```

`maxCapacity` limits the number of registered connections and is capped at 65535. `initialReserve` reserves storage for up to that many connections within the configured capacity. A negative constructor argument terminates the process. `add(hConn)` requires an initialized network and a handle recognized by GameNetworkingSockets. It returns the assigned ID, or `std::nullopt` when the network is unavailable, the handle is invalid or already registered, or the manager is at capacity. `remove(id)` unregisters an ID and makes it available for reuse. A missing or out-of-range ID has no effect. Closing the underlying connection is handled by the networking component. The constructor, `add()`, and `remove()` can propagate allocation exceptions. If registration or removal fails during allocation, the existing mappings remain intact.

Copying a manager produces independent registrations. If copy assignment throws, the destination preserves its mappings, count, capacity and ID allocation state. A successful move transfers the registrations and ID allocation state. The source becomes empty, retains its configured maximum capacity and starts assigning IDs at 1 again.

`getHSteamNetConnection` looks up a handle by its internal ID and returns `k_invalidConnection` when there is no registration for the ID. `getClientId` looks up the ID in this manager's local registration map, independently of GNS userdata or whether the handle has since closed. It returns `std::nullopt` for an absent registration. `getOnlineCount` counts registered handles, including closed ones still registered with this manager.

`forEach` calls the callback for each active registration, providing the ID and handle:

```cpp
connections.forEach(
    [](std::uint16_t id, lightnerx::network::ConnectionHandle connection) {
        // Send to or inspect the connection.
    }
);
```

## `<lightnerx/network/transport.hpp>`

### Direct sending

```cpp
lightnerx::Result<void> sendToConnection(
    std::string_view message,
    lightnerx::network::ConnectionHandle hConn
);
```

Sends the bytes in `message` to `hConn` through GameNetworkingSockets (GNS) using reliable delivery. This call bypasses the batcher and accepts raw bytes. Success means the transport accepted the message for sending.

| Error | Condition |
| --- | --- |
| `ErrorCode::ResourceUnavailableTryAgain` | The network is uninitialized or unavailable. |
| `ErrorCode::NotConnected` | The handle equals `k_invalidConnection`, or the transport interface is no longer available after checking initialization. |
| `ErrorCode::MessageSize` | The byte count exceeds the 32-bit length field, after the initial network and handle checks. |
| `ErrorCode::IoError` | GNS rejected the send, for example because the connection ended, its send buffer is full, or the message exceeds the GNS send limit. |

For IP connections, the bundled GNS version limits each message to 512 KiB (524,288 bytes). A payload above that limit that still fits in the 32-bit length field reaches GNS and returns `ErrorCode::IoError`. When sending INER frames, apply the protocol's header and payload limits as well. See the [GNS send limit](https://github.com/ValveSoftware/GameNetworkingSockets/blob/d534c19aa760df3fb75fd20db13ba1932b8a5463/include/steam/steamnetworkingtypes.h#L843-L846) and the [INER binary representation](communication.md#binary-representation).

### Sending to a group

```cpp
lightnerx::Result<void> sendToConnections(
    std::string_view message,
    const std::vector<lightnerx::network::ConnectionHandle> connections
);
```

Forwards the same bytes to each handle in vector order, including repeated handles. Attempts all destinations and returns the first error from `sendToConnection()`, if any. An empty vector succeeds without sending. Copying a vector passed by lvalue can propagate `std::bad_alloc` before sending.

### Broadcast

```cpp
lightnerx::Result<void> broadcast(
    std::string_view message,
    const lightnerx::network::ConnectionManager& connectionManager,
    const std::vector<lightnerx::network::ConnectionHandle> exceptions
);
```

Forwards the same bytes to the connections registered in `connectionManager`, excluding the handles in `exceptions`. Attempts all selected destinations and returns the first error from `sendToConnection()`, if any. An empty selection succeeds without sending. Copying an exceptions vector passed by lvalue can propagate `std::bad_alloc` before sending. Pass `{}` as the required third argument to include all registered handles.

```cpp
const auto all = lightnerx::network::broadcast(bytes, connections, {});
const auto others = lightnerx::network::broadcast(bytes, connections, {connectionToSkip});
```

The examples above are independent calls; check the result of each send used by the application. To defer sending until a flush, use `Target::broadcast` with `queueMessage`.

## `<lightnerx/network/target.hpp>`

### `lightnerx::network::Target`

Represents the destination of a queued message. Use the static factories to create the destination:

```cpp
static Target single(lightnerx::network::ConnectionHandle conn);

static Target group(std::vector<lightnerx::network::ConnectionHandle> connections);

static Target broadcast(
    std::vector<lightnerx::network::ConnectionHandle> exceptions = {}
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
    std::vector<lightnerx::network::ConnectionHandle>,
    lightnerx::network::ConnectionHandle
> connection;

std::vector<lightnerx::network::ConnectionHandle> broadcastExceptions;
```

## `<lightnerx/network/message_batch.hpp>`

### `lightnerx::network::OutboundMessage`

```cpp
static lightnerx::Result<OutboundMessage> create(
    std::string_view header, std::string_view payload, Target target
);

std::string_view data() const noexcept;
```

`create(header, payload, target)` copies the two byte views in order and stores the destination by value. Initialize the network before calling it. It returns the same allocation errors as the outbound converters. `data()` borrows the resulting bytes while the object retains its buffer and returns an empty view for an empty buffer or a moved-from message.

### Batcher

```cpp
lightnerx::Result<void> initMessagesBatcher(int capacity);
void clearMessagesBatcher() noexcept;

lightnerx::Result<void> queueMessage(OutboundMessage message);
lightnerx::Result<void> queueMessage(lightnerx::Result<OutboundMessage> message);

lightnerx::Result<void> flushAll(
    const ConnectionManager& connectionManager
);
```

`initMessagesBatcher(capacity)` ensures room for at least `capacity` pending messages in the shared queue. Capacity counts messages. Reserving preserves the current messages and never reduces the reserved capacity. The first access constructs the queue, including when a consumer reserves capacity before `main()`. A negative value returns `ErrorCode::InvalidArgument`. A value above the queue's maximum representable size returns `ErrorCode::NotEnoughMemory`. If allocating the requested storage fails, `std::bad_alloc` can propagate before the function returns a result.

`queueMessage(OutboundMessage)` consumes the message and moves it into the queue without copying its bytes. A full queue releases the message and returns `ErrorCode::NotEnoughMemory`. A moved-from message returns `ErrorCode::InvalidArgument`. The overload taking `Result<OutboundMessage>` propagates the converter's error or consumes and queues the contained message.

`flushAll` processes pending messages in queue order. Each message is sent to its individual connection, group, or broadcast recipients. The required `connectionManager` provides broadcast recipients at flush time. With returned send errors, flushing still attempts every entry and returns the error from the last failed entry. Within a group or broadcast entry, the first recipient error represents that entry. If preparing a send throws, for example while copying a destination vector, the exception propagates and pending messages are released during unwinding. An empty queue returns success. After any flush, the queue is empty and its reserved capacity is preserved.

`OutboundMessage` has move-only ownership and releases its buffer on destruction. Pass a named message or converter result with `std::move(outbound)`, or pass the converter's result directly to `queueMessage`.

With the network and batcher initialized, and `event`, `payload`, `connection`, and `connections` defined by the application, typical usage after a converter is:

```cpp
auto outbound = lightnerx::communication::event2OutboundMessage(
    event,
    payload,
    lightnerx::network::Target::single(connection)
);

const auto queued = lightnerx::network::queueMessage(std::move(outbound));

if (!queued) {
    std::cout << "Failed to queue: " << lightnerx::errorMessage(queued.error()) << '\n';
} else if (const auto flushed = lightnerx::network::flushAll(connections); !flushed) {
    std::cout << "Failed to send: " << lightnerx::errorMessage(flushed.error()) << '\n';
}
```

You can also queue multiple messages before calling `flushAll`. The batcher keeps each message separate, and the application chooses when to flush.

## Validation and callback behavior

Both interpretation methods accept exactly one complete message. The declared total size must equal the supplied view and include the entire header. Authority sizes are decoded from 15 bits (maximum 32767 bytes); Obedient sizes use 16 bits (maximum 65535 bytes). Truncated headers, mismatched sizes, unknown response status and unregistered Obedient origins produce no callback, matching the existing void parser interface. There is no incremental or concatenated-message framing.

Callbacks run synchronously. Registration during a callback is safe. Newly registered callbacks participate from the next dispatch, including a nested interpretation. Existing callback objects retain mutable state. An exception escaping a callback during transport connection status dispatch terminates the process. Receive and interpretation callbacks can propagate exceptions to the caller, and acquired receive buffers are released during unwinding.

Recursive `update()` returns `ErrorCode::OperationInProgress`. Callbacks may disconnect or request `Server::close()`; do not destroy or assign the component executing a callback. Referenced objects must remain alive until dispatch finishes. A negative receive result returns `ErrorCode::IoError`, distinct from an empty queue; the client also clears and closes its failed connection. Updating an unstarted/closed wrapper returns `ErrorCode::NotConnected`.

`ConnectionManager` owns an independent, bidirectional mapping and leaves GNS userdata available to the application. Different managers may independently register the same valid handle. IDs range from 1 to 65535 and are recycled. `forEach` visits the index range present on entry in ascending ID order. It safely allows addition and removal during iteration and may observe replacements within the original range. An exception from the callback stops the traversal and propagates to the caller.

The batch queue is global. `clearMessagesBatcher()` releases pending messages and preserves capacity. Actual network shutdown and queue destruction also release pending messages. Serialize all batch operations, including clearing and shutdown.
