# Communication and the INER protocol

## INER and LightnerX

INER is a semantic protocol. It defines the meaning and organization of messages, but does not define sockets, addressing, threads, or the transmission mechanism.

LightnerX implements the protocol's message types and provides networking components based on GameNetworkingSockets. The division is:

| Responsibility | INER | LightnerX |
| --- | --- | --- |
| Meaning of Event, Response, Intention, and Notification | Yes | Represented by C++ types |
| Message identification | Yes | `MBT::type`, `correlationId`, and `status` fields |
| Header encoding | Yes | Packed structures and converters |
| Sockets and connections | No | `Server`, `Client`, and GameNetworkingSockets |
| Execution model | No | Chosen by the application |

## Roles

### Authority

The Authority controls internal states. It receives `Intention` and `Notification` from the Obedient and sends `Event` and `Response`.

In LightnerX, the server typically interprets received messages with:

```cpp
interpreter.obedientInterpret(bytes, registry, connection, connections);
```

### Obedient

The Obedient sends intentions to the Authority and receives information about occurrences and results.

In LightnerX, the client typically interprets received messages with:

```cpp
interpreter.authorityInterpret(bytes);
```

## Message types

### Event

`Event` communicates an occurrence from the Authority. Its header contains:

- `BaseProtocol::size`;
- `MBT::type`, used as `type_id`.

The payload, when present, follows the header.

### Response

`Response` relates the result to an intention through `correlationId` and contains a `status`.

The statuses are exposed by constants in the `lightnerx.communication.response` module:

| Constant | Value | Meaning | Type used by LightnerX |
| --- | ---: | --- | --- |
| `APPROVED_RESPONSE` | `0` | intention fully approved | `Response` |
| `DENIED_RESPONSE` | `1` | intention denied | `UnsuccessfulResponse` |
| `PARTIALLY_APPROVED_RESPONSE` | `2` | intention partially applied | `UnsuccessfulResponse` |
| `FAILURE_RESPONSE` | `3` | internal error during processing | `Response` |

A failure does not necessarily mean that the intention was denied; the application may decide to try again. `UnsuccessfulResponse` adds `reasonCode` to the header of denied or partially approved responses. For partial approval, the meaning of the payload is defined by the application.

### Intention

`Intention` requests that the Authority perform an action. Its header contains:

- `BaseProtocol::size`;
- `MBT::type`;
- `correlationId`.

The intention's payload follows this header.

### Notification

`Notification` communicates information to the Authority without requiring state validation, since it does not request a change to the Authority's internal states. Its header contains `size` and `type`, followed by the optional payload.

On the server, `NotificationRegistry` specifies which `type_id` values should be interpreted as `Notification`:

```cpp
import std;
import lightnerx.communication.notification;

void registerNotifications(lightnerx::communication::NotificationRegistry& registry) {
    registry.registerId(47);
    registry.registerIds(std::unordered_set<std::uint16_t>{99, 100});
}
```

The `type_id` values are defined by the application. There is no universal table of INER types. When receiving messages from the Obedient, registered IDs identify notifications; the others identify intentions. To send notifications, use `notification2OutboundMessage`.

## Binary representation

Header structures are packed without additional padding using `#pragma pack(push, 1)`. The converters copy fields in the machine's native representation; participants must use a compatible representation for these fields and agree on the payload format.

### Messages from the Authority

The first field uses a combined value: the least significant bit indicates whether the message is a `Response`, and the remaining fifteen bits represent the size.

The table describes the recommended formats for emission: approved, denied and failure responses have no payload; partial approval may carry application-defined bytes.

| Message | Header | Recommended content when sending |
| --- | --- | --- |
| `Event` | 2 bytes of combined `size` + 2 bytes of `type` | variable payload |
| `Response` | 2 bytes of combined `size` + 2 bytes of `correlationId` + 1 byte of `status` | no payload for the statuses represented by `Response` |
| `UnsuccessfulResponse` | `Response` structure + 2 bytes of `reasonCode` | variable payload for a partial response |

When constructing the header, the combined value takes the form:

```text
Event:    (total_size << 1) | 0
Response: (total_size << 1) | 1
```

The interpreter retrieves the size with `base >> 1` and distinguishes `Event` from `Response` with `base & 1`. In the headers delivered to the callback, `protocol.size` already contains the decoded size, without the type bit.

Reception currently tolerates additional bytes in responses when the declared total matches the supplied span and the selected header is complete. For approved and failure responses, the interpreter ignores those bytes and delivers an empty payload. For denied and partially approved responses, it delivers all bytes after `UnsuccessfulResponse`, even though the recommended denied-response format has no payload. `protocol.size` still includes the complete received message, including any ignored bytes; it can therefore exceed the header size plus the delivered payload size. This reception behavior does not change the recommended emission formats. Converters copy the supplied payload without enforcing status-specific payload rules.

### Messages from the Obedient

In messages from the Obedient, `size` directly represents the total message size.

| Message | Header | Content |
| --- | --- | --- |
| `Notification` | 2 bytes of `size` + 2 bytes of `type` | variable payload |
| `Intention` | 2 bytes of `size` + 2 bytes of `type` + 2 bytes of `correlationId` | variable payload |

The specified size must include the header and payload. Each interpretation call must receive exactly one complete message: the decoded total must equal the supplied span and the header must be complete, otherwise no callback is delivered. Authority messages are limited to 32767 bytes and Obedient messages to 65535 bytes. The interpreter does not split concatenated messages or buffer partial frames. Unknown response statuses are ignored. The converters copy the header and payload into a `SteamNetworkingMessage_t`; they do not automatically construct the semantic value of the `size` field. They throw `std::bad_alloc` if GNS allocation fails and `std::length_error` if the allocation size cannot be represented.

## Identifiers

- `type_id` identifies the semantic type defined by the application and appears in the code as `MBT::type`.
- `correlationId` relates a response to its corresponding intention.
- The ID received in the server's interpreted message callback is the internal identifier assigned by `ConnectionManager`.

This internal ID belongs to the connection management implementation and is not an INER protocol field. The name `originId` does not designate a protocol field either. Network destinations use `HSteamNetConnection` handles, which are distinct from internal IDs and semantic identifiers.

## Network destinations

`lightnerx::network::Target`, exported by the `lightnerx.network.target` module, defines where a queued message will be sent. The static factories create the destination description:

| Factory | Destination |
| --- | --- |
| `Target::single(connection)` | a specific connection |
| `Target::group(connections)` | all connections in the provided vector |
| `Target::broadcast()` | all connections registered in `ConnectionManager` |
| `Target::broadcast(exceptions)` | all connections registered in `ConnectionManager`, except the specified handles |

`Target::group` stores the explicit selection of handles. Broadcast queries the `ConnectionManager` passed to `flushAll` at flush time, using the mappings maintained by the application. Its exceptions are also `HSteamNetConnection` handles.

## Receiving flow

```text
Network -> Server callback -> obedientInterpret -> application callback
Network -> Client callback -> authorityInterpret -> application callback
```

`MessageInterpreter` delivers a `lightnerx::communication::Message` whose `header` contains the concrete type in a `std::variant` and whose `payload` is a `std::span<const std::byte>`.

Callbacks run during the interpretation call. The header is delivered by value, but the span only references the received bytes. In the flow with `Server` or `Client`, these bytes are released after the receive callbacks. To retain the payload, copy its bytes during the callback; copying only `Message` keeps a reference to the same buffer.

## Sending flow

The usual flow is:

1. Initialize the network and call `initMessagesBatcher(capacity)` before queuing the first message.
2. Construct the header with the total message size.
3. Create a `lightnerx::network::Target`.
4. Use the corresponding converter to obtain a `lightnerx::network::OutboundMessage`.
5. Pass the object to `queueMessage(std::move(outbound))`, transferring ownership of the message and destination.
6. Check the queuing result and call `flushAll(connectionManager)` at the time chosen by the application. Releasing the message becomes the batcher's responsibility, including when queuing returns an error.

Example of an `Event` from the Authority with one byte of payload defined by the application. The function assumes that the network and batcher have been initialized and supports broadcast with or without exceptions:

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

    const auto queued = lightnerx::network::queueMessage(std::move(outbound));

    if (!queued) {
        return queued;
    }
    return lightnerx::network::flushAll(connections);
}
```

Call `broadcastEvent(connections)` for everyone or `broadcastEvent(connections, {connectionToSkip})` to exclude a handle, checking the returned `std::expected`.

### What each step does

`queueMessage` receives the `OutboundMessage` from the converter and stores the same message with its destination, without allocating another message or copying the bytes. The call consumes the message: on success, the batcher keeps it until the flush; if the queue is full, it releases it immediately. The caller must not call `Release()` or reuse the message after passing it to the queue. A null pointer is rejected with `invalid_argument`.

`flushAll` iterates over pending messages, routes each one according to its destination, and releases the message received from the converter with `Release()`. All entries are processed and the queue is emptied even when send errors occur. The return value indicates success or the last error encountered among these operations.

Batching lets you accumulate multiple messages before a flush. Each entry remains a separate message; payloads are not concatenated. The application decides when to flush, for example at the end of an update. Success indicates that the transport accepted the send operations, without confirming processing by the recipient.

For `Event` and `Response`, the `size` field must use the combined value described above. For `Intention` and `Notification`, it uses the total size directly.

Pending batches can be discarded explicitly with `clearMessagesBatcher()` and are also released at actual network shutdown. A thrown send exception releases the complete queue before propagating to the caller.
