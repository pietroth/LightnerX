![LightnerX](docs/LightnerX%20Framed%20Wordmark.png)

LightnerX is a C++17 library that abstracts client-server networking behind a simple API. It lets applications open and close servers, manage client connections, and exchange messages without calling GameNetworkingSockets directly. It also provides the in-memory message structures and interpreters needed to use the INER semantic protocol.

> [!WARNING]
> **Development Notice**
>
> LightnerX was conceived, designed in its entirety, and originally developed by humans. After the migration to C++17, the project relied too heavily on AI coding agents to make development easier and faster.
>
> A comprehensive human-led technical review is now underway to regain direct control over the project's architecture. The review covers the architecture, implementation, tests, and documentation. It will revisit every architectural or design decision that AI may have influenced and verify that AI-generated code works as intended.
>
> New feature development is temporarily paused during the review. Bug reports and technical feedback remain welcome.
>
> Going forward, LightnerX will use AI much more selectively, reserving it for tasks where assistance is needed, such as repetitive work. Every AI-generated change will undergo a much stricter human review and will be reworked as extensively as needed before it is integrated.

## Features

- Start and close servers with `Server::start()` and `Server::close()`, and disconnect individual clients with `Server::disconnectConnection()`.
- Initialize clients with `Client::start()`, connect with `Client::connect()`, and disconnect with `Client::disconnect()`. Destroying the client releases its networking resources.
- Send and receive all four INER message types. Clients send `Intention` and `Notification` messages. Servers send `Event` and `Response` messages.
- Prepare outgoing messages by combining an INER header and payload into a send buffer with converters such as `event2OutboundMessage()`.
- Decode incoming bytes with `MessageInterpreter` and deliver structured messages to application callbacks.

## Why LightnerX?

LightnerX keeps networking simple in applications built around an authoritative server. Its modular components let the application decide how to connect networking, connection management, and message handling. INER defines what each message means and which side sends it, giving the server and its clients a shared communication model.

Build with CMake 3.22 or newer and a C++17 compiler. The CMake project provides the `LightnerX` library target without producing or requiring a standalone executable. Public headers live under `include/lightnerx/`. Byte views use `std::string_view` and results use `lightnerx::Result`. See [Setup](docs/setup.md) for dependencies and tested toolchains.

## Library, not a framework

LightnerX provides reusable components. The consuming application decides:

- when to initialize and shut down components;
- how to run `update()`;
- which threads to use;
- how to organize the connection lifecycle;
- which components to use or replace with its own implementations.

For example, you can pass one complete INER message from your own transport to `lightnerx::communication::MessageInterpreter`. `authorityInterpret` does not require a connection. `obedientInterpret` requires a `lightnerx::network::ConnectionManager` registration: `add()` accepts only live GNS handles, so arbitrary identifiers from another transport are not supported by that path. The manager stores its own mapping and does not use GNS connection userdata.

## Key concepts

- **Authority:** controls internal states and receives intentions from the Obedient.
- **Obedient:** sends intentions to the Authority and receives events and responses.
- **Event:** an occurrence communicated by the Authority.
- **Response:** the result of an intention sent by the Obedient.
- **Intention:** an action request sent to the Authority.
- **Notification:** information sent to the Authority without requesting a state change.

### INER message flow

The INER protocol defines four message types exchanged between the Authority and the Obedient:

Choose the message type according to its purpose and direction. In a typical client-server application, the server acts as the Authority and the client as the Obedient.

| Message type | Direction | When to use it | Examples |
| --- | --- | --- | --- |
| **Intention** | Client → Server | Request an action that the Authority must evaluate. | Move a player, fire a weapon, use an item, join a match. |
| **Notification** | Client → Server | Report information without requesting a change to the Authority's internal state. | Report measured ping/latency, FPS, or client diagnostics. |
| **Event** | Server → Client | Communicate an occurrence or state update from the Authority. | A player joined, an entity moved, a projectile spawned, a match started. |
| **Response** | Server → Client | Return the outcome of a specific Intention, linked by its `correlationId`. | Movement approved, firing denied because ammunition is insufficient, an action partially applied, or processing failed. |

These examples describe application-defined messages; LightnerX does not provide built-in gameplay commands or telemetry formats. A Response reports an Intention's outcome, while an Event communicates an occurrence to the relevant clients.

```mermaid
flowchart LR
    A["🖥️ AUTHORITY<br/>Server"]
    O["💻 OBEDIENT<br/>Client"]

    A -->|EVENT| O
    A -->|RESPONSE| O
    O -->|INTENTION| A
    O -->|NOTIFICATION| A

    classDef authority fill:#123456,stroke:#2699ff,color:#fff,stroke-width:2px
    classDef obedient fill:#302050,stroke:#a76bee,color:#fff,stroke-width:2px

    class A authority
    class O obedient

    linkStyle 0 stroke:#2596ee,stroke-width:3px
    linkStyle 1 stroke:#16a085,stroke-width:3px
    linkStyle 2 stroke:#e69b27,stroke-width:3px
    linkStyle 3 stroke:#a76bee,stroke-width:3px
```

In LightnerX, the application connects components through callbacks: the network delivers bytes, and `MessageInterpreter` interprets those bytes according to the direction of communication. When sending, `lightnerx::network::Target` lets you select a connection, a group, or a broadcast with or without exceptions.

## Navigation

- [Setup](docs/setup.md)
- [First steps](docs/first-steps.md)
- [Communication and the INER protocol](docs/communication.md)
- [API reference](docs/api.md)
- [C++17 byte views and results](docs/cpp17-types.md)
- [Migration and validation](docs/migration-cpp17.md)

## Server example

This example receives notifications from the client and sends the text `Hello, World!` in events to registered connections. The snippets below make up the complete program at the end of the README; the statements belong inside the main function, after the includes shown in the complete code.

### Create the components

The manager maps handles to internal client IDs. Register ID `47` so that the interpreter recognizes notifications sent by the client in this example.

```cpp
using namespace std::chrono_literals;
namespace network = lightnerx::network;
namespace communication = lightnerx::communication;

network::Server server;
network::ConnectionManager connections{30, 30};
communication::MessageInterpreter interpreter;
communication::NotificationRegistry notifications;
notifications.registerId(47);
```

### Track connections

The callbacks keep the manager up to date. If registering a connection fails, the server requests its disconnection.

```cpp
server.addClientConnectedCallback([&](lightnerx::network::ConnectionHandle connection) {
    if (!connections.add(connection)) {
        if (const auto result = server.disconnectConnection(connection, false); !result) {
            std::cout << "Failed to disconnect: " << lightnerx::errorMessage(result.error()) << '\n';
        }
    }
});

server.addClientDisconnectedCallback([&](lightnerx::network::ConnectionHandle connection) {
    if (const auto id = connections.getClientId(connection)) {
        connections.remove(*id);
    }
});
```

### Receive messages

Forward the received bytes to `obedientInterpret`. The interpreter's callback delivers the message and the internal client ID. In this example, the payload is text and is read during the callback.

```cpp
server.addClientReceivedMessageCallback(
    [&](std::string_view bytes, lightnerx::network::ConnectionHandle connection) {
        interpreter.obedientInterpret(bytes, notifications, connection, connections);
    }
);

interpreter.addObedientInterpretedMessageCallback(
    [](communication::Message message, std::uint16_t clientId) {
        const auto text = message.payload;
        std::cout << "Client " << clientId << ": " << text << '\n';
    }
);
```

### Start the server

Start listening on port `41333`, with an established connection timeout of `3000` ms, and reserve one slot for the batcher's pending message.

```cpp
if (const auto result = server.start(41333, 3000); !result) {
    std::cout << "Failed to start: " << lightnerx::errorMessage(result.error()) << '\n';
    return 1;
}
if (const auto result = network::initMessagesBatcher(1); !result) {
    std::cout << "Failed to reserve the batch: " << lightnerx::errorMessage(result.error()) << '\n';
    return 1;
}
```

### Prepare an event

Use the text view as the payload and include the header and payload in the total size. For `Event`, the size is shifted one bit to the left. Type `990` is chosen by the application.

```cpp
const std::string_view payload = "Hello, World!";
const communication::Event event{
    // The least significant bit distinguishes Event from Response.
    static_cast<std::uint16_t>((sizeof(communication::Event) + payload.size()) << 1),
    990
};
```

### Update and send

`update()` processes the network. Pass the converter's result directly to `queueMessage`. A conversion error is returned to the caller. `Target::broadcast()` selects the connections registered at flush time.

```cpp
while (true) {
    if (const auto result = server.update(); !result) {
        std::cout << "Failed to update: " << lightnerx::errorMessage(result.error()) << '\n';
        break;
    }
    if (const auto result = network::queueMessage(communication::event2OutboundMessage(
            event, payload, network::Target::broadcast())); !result) {
        std::cout << "Failed to queue: " << lightnerx::errorMessage(result.error()) << '\n';
        break;
    }
    if (const auto result = network::flushAll(connections); !result) {
        std::cout << "Failed to send: " << lightnerx::errorMessage(result.error()) << '\n';
        break;
    }
    std::this_thread::sleep_for(3s);
}
```

`queueMessage` takes ownership of the message, including when it returns an error. `flushAll` processes the queue and releases the messages. The three-second interval is a choice made for this example; the application defines its own update rate.

### Shut down the server

After exiting the loop due to an error, close the server. Returning `1` signals an exit due to failure in this example.

```cpp
server.close();
return 1;
```

## Client example

The client connects to the server at `127.0.0.1:41333`, prints the received events, and sends a notification with the same text. Run the server and client in separate processes.

### Create the components

The empty manager satisfies the `flushAll` signature. Sending to the server individually uses the client's connection handle directly.

```cpp
using namespace std::chrono_literals;
namespace network = lightnerx::network;
namespace communication = lightnerx::communication;

network::Client client;
network::ConnectionManager connections{0, 0}; // Required by flush, without broadcast.
communication::MessageInterpreter interpreter;
```

### Receive messages from the server

Forward the bytes to `authorityInterpret`, which interprets messages from the Authority. The callback below prints the text payload agreed upon by these examples and handles empty payloads.

```cpp
client.addServerReceivedMessageCallback([&](std::string_view bytes) {
    interpreter.authorityInterpret(bytes);
});

interpreter.addAuthorityInterpretedMessageCallback([](communication::Message message) {
    const auto text = message.payload;
    std::cout << "Server: " << text << '\n';
});
```

The callbacks use the bytes during the call. To retain a received payload, copy its bytes into application storage before returning.

### Start and connect

Initialize the client, request a connection to the server, and reserve space in the batcher. The connection will be tracked during calls to `update()`.

```cpp
if (const auto result = client.start(); !result) {
    std::cout << "Failed to start: " << lightnerx::errorMessage(result.error()) << '\n';
    return 1;
}
if (const auto result = client.connect("127.0.0.1", 41333); !result) {
    std::cout << "Failed to connect: " << lightnerx::errorMessage(result.error()) << '\n';
    return 1;
}
if (const auto result = network::initMessagesBatcher(1); !result) {
    std::cout << "Failed to reserve the batch: " << lightnerx::errorMessage(result.error()) << '\n';
    return 1;
}
```

### Prepare a notification

`Notification` uses the total size directly, without bit shifting. ID `47` corresponds to the registration made on the server.

```cpp
const std::string_view payload = "Hello, World!";
const communication::Notification notification{
    static_cast<std::uint16_t>(sizeof(communication::Notification) + payload.size()),
    47 // The server must register this ID in NotificationRegistry.
};
```

### Update and send to the server

Process up to 32 messages per update and send when `isConnected()` indicates an established connection. `Target::single` selects the server's handle. The handle can become invalid during a successful update when the peer closes the connection. The example exits when that happens and gives a pending attempt ten seconds to connect.

```cpp
const auto connectDeadline = std::chrono::steady_clock::now() + 10s;
bool connectedOnce = false;
while (true) {
    if (const auto result = client.update(32); !result) {
        std::cout << "Failed to update: " << lightnerx::errorMessage(result.error()) << '\n';
        break;
    }
    if (client.getServerHConn() == network::k_invalidConnection) {
        std::cout << (connectedOnce ? "Connection closed\n" : "Connection attempt failed\n");
        return connectedOnce ? 0 : 1;
    }
    if (client.isConnected()) {
        connectedOnce = true;
        if (const auto result = network::queueMessage(communication::notification2OutboundMessage(
                notification, payload, network::Target::single(client.getServerHConn()))); !result) {
            std::cout << "Failed to queue: " << lightnerx::errorMessage(result.error()) << '\n';
            break;
        }
        if (const auto result = network::flushAll(connections); !result) {
            std::cout << "Failed to send: " << lightnerx::errorMessage(result.error()) << '\n';
            break;
        }
    } else if (!connectedOnce && std::chrono::steady_clock::now() >= connectDeadline) {
        std::cout << "Connection attempt timed out\n";
        break;
    }
    std::this_thread::sleep_for(3000ms);
}
```

### Shut down the client

On exit, the `Client` destructor closes any remaining connection and shuts down the network. Returning `1` indicates a failure during connection establishment or message processing.

```cpp
return 1; // The Client destructor closes the connection and shuts down the network.
```

## Complete code

The programs below bring together the snippets presented above. The text payload, IDs, loop, and intervals are choices made by the consuming application.

### Complete server

```cpp

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <thread>

#include <lightnerx/network/server.hpp>
#include <lightnerx/network/connection_manager.hpp>
#include <lightnerx/network/message_batch.hpp>
#include <lightnerx/network/target.hpp>
#include <lightnerx/communication/message.hpp>
#include <lightnerx/communication/notification.hpp>
#include <lightnerx/communication/event.hpp>

int main() {
    using namespace std::chrono_literals;
    namespace network = lightnerx::network;
    namespace communication = lightnerx::communication;

    network::Server server;
    network::ConnectionManager connections{30, 30};
    communication::MessageInterpreter interpreter;
    communication::NotificationRegistry notifications;
    notifications.registerId(47);

    server.addClientConnectedCallback([&](lightnerx::network::ConnectionHandle connection) {
        if (!connections.add(connection)) {
            if (const auto result = server.disconnectConnection(connection, false); !result) {
                std::cout << "Failed to disconnect: " << lightnerx::errorMessage(result.error()) << '\n';
            }
        }
    });

    server.addClientDisconnectedCallback([&](lightnerx::network::ConnectionHandle connection) {
        if (const auto id = connections.getClientId(connection)) {
            connections.remove(*id);
        }
    });

    server.addClientReceivedMessageCallback(
        [&](std::string_view bytes, lightnerx::network::ConnectionHandle connection) {
            interpreter.obedientInterpret(bytes, notifications, connection, connections);
        }
    );

    interpreter.addObedientInterpretedMessageCallback(
        [](communication::Message message, std::uint16_t clientId) {
            const auto text = message.payload;
            std::cout << "Client " << clientId << ": " << text << '\n';
        }
    );

    if (const auto result = server.start(41333, 3000); !result) {
        std::cout << "Failed to start: " << lightnerx::errorMessage(result.error()) << '\n';
        return 1;
    }
    if (const auto result = network::initMessagesBatcher(1); !result) {
        std::cout << "Failed to reserve the batch: " << lightnerx::errorMessage(result.error()) << '\n';
        return 1;
    }

    const std::string_view payload = "Hello, World!";
    const communication::Event event{
        // The least significant bit distinguishes Event from Response.
        static_cast<std::uint16_t>((sizeof(communication::Event) + payload.size()) << 1),
        990
    };

    while (true) {
        if (const auto result = server.update(); !result) {
            std::cout << "Failed to update: " << lightnerx::errorMessage(result.error()) << '\n';
            break;
        }
        if (const auto result = network::queueMessage(communication::event2OutboundMessage(
                event, payload, network::Target::broadcast())); !result) {
            std::cout << "Failed to queue: " << lightnerx::errorMessage(result.error()) << '\n';
            break;
        }
        if (const auto result = network::flushAll(connections); !result) {
            std::cout << "Failed to send: " << lightnerx::errorMessage(result.error()) << '\n';
            break;
        }
        std::this_thread::sleep_for(3s);
    }

    server.close();
    return 1;
}
```

### Complete client

```cpp

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <thread>

#include <lightnerx/network/client.hpp>
#include <lightnerx/network/connection_handle.hpp>
#include <lightnerx/network/connection_manager.hpp>
#include <lightnerx/network/message_batch.hpp>
#include <lightnerx/network/target.hpp>
#include <lightnerx/communication/message.hpp>
#include <lightnerx/communication/notification.hpp>

int main() {
    using namespace std::chrono_literals;
    namespace network = lightnerx::network;
    namespace communication = lightnerx::communication;

    network::Client client;
    network::ConnectionManager connections{0, 0}; // Required by flush, without broadcast.
    communication::MessageInterpreter interpreter;

    client.addServerReceivedMessageCallback([&](std::string_view bytes) {
        interpreter.authorityInterpret(bytes);
    });

    interpreter.addAuthorityInterpretedMessageCallback([](communication::Message message) {
        const auto text = message.payload;
        std::cout << "Server: " << text << '\n';
    });

    if (const auto result = client.start(); !result) {
        std::cout << "Failed to start: " << lightnerx::errorMessage(result.error()) << '\n';
        return 1;
    }
    if (const auto result = client.connect("127.0.0.1", 41333); !result) {
        std::cout << "Failed to connect: " << lightnerx::errorMessage(result.error()) << '\n';
        return 1;
    }
    if (const auto result = network::initMessagesBatcher(1); !result) {
        std::cout << "Failed to reserve the batch: " << lightnerx::errorMessage(result.error()) << '\n';
        return 1;
    }

    const std::string_view payload = "Hello, World!";
    const communication::Notification notification{
        static_cast<std::uint16_t>(sizeof(communication::Notification) + payload.size()),
        47 // The server must register this ID in NotificationRegistry.
    };

    const auto connectDeadline = std::chrono::steady_clock::now() + 10s;
    bool connectedOnce = false;
    while (true) {
        if (const auto result = client.update(32); !result) {
            std::cout << "Failed to update: " << lightnerx::errorMessage(result.error()) << '\n';
            break;
        }
        if (client.getServerHConn() == network::k_invalidConnection) {
            std::cout << (connectedOnce ? "Connection closed\n" : "Connection attempt failed\n");
            return connectedOnce ? 0 : 1;
        }
        if (client.isConnected()) {
            connectedOnce = true;
            if (const auto result = network::queueMessage(communication::notification2OutboundMessage(
                    notification, payload, network::Target::single(client.getServerHConn()))); !result) {
                std::cout << "Failed to queue: " << lightnerx::errorMessage(result.error()) << '\n';
                break;
            }
            if (const auto result = network::flushAll(connections); !result) {
                std::cout << "Failed to send: " << lightnerx::errorMessage(result.error()) << '\n';
                break;
            }
        } else if (!connectedOnce && std::chrono::steady_clock::now() >= connectDeadline) {
            std::cout << "Connection attempt timed out\n";
            break;
        }
        std::this_thread::sleep_for(3000ms);
    }

    return 1; // The Client destructor closes the connection and shuts down the network.
}
```

## Runtime contracts

One started `Client` or `Server` owns networking when the process uses a single LightnerX library instance. Applications with multiple plugins should link them to the same shared LightnerX library. Both classes have stable addresses and cannot be copied or moved. `Server::close()` and either owner's destructor release the runtime; a client can disconnect and reconnect without being destroyed. Direct `networkInit()` / `networkKill()` remain available when using the transport without a wrapper. See [API reference](docs/api.md) for shutdown, callback lifetime, reentrancy and message validation contracts.
