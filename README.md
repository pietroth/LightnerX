# LightnerX

LightnerX is a modular static C++ library for building applications that communicate using the INER semantic protocol. The CMake project provides the `LightnerX` target; it neither produces nor requires its own executable. The library provides types for representing messages, interpreters, callbacks, connection management, and a transport layer based on GameNetworkingSockets.

INER defines the meaning of messages exchanged between an Authority and an Obedient. It does not define sockets, connections, or an execution model. These capabilities are provided by LightnerX's networking components and can be connected to the application according to the developer's needs.

## Library, not a framework

LightnerX provides reusable components. The consuming application decides:

- when to initialize and shut down components;
- how to run `update()`;
- which threads to use;
- how to organize the connection lifecycle;
- which modules to use or replace with its own implementations.

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

## Server example

This example receives notifications from the client and sends the text `Hello, World!` in events to registered connections. The snippets below make up the complete program at the end of the README; the statements belong inside the main function, after the imports shown in the complete code.

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
server.addClientConnectedCallback([&](HSteamNetConnection connection) {
    if (!connections.add(connection)) {
        if (const auto result = server.disconnectConnection(connection, false); !result) {
            std::println("Failed to disconnect: {}", result.error().message());
        }
    }
});

server.addClientDisconnectedCallback([&](HSteamNetConnection connection) {
    if (const auto id = connections.getClientId(connection)) {
        connections.remove(*id);
    }
});
```

### Receive messages

Forward the received bytes to `obedientInterpret`. The interpreter's callback delivers the message and the internal client ID. In this example, the payload is text and is read during the callback.

```cpp
server.addClientReceivedMessageCallback(
    [&](std::span<const std::byte> bytes, HSteamNetConnection connection) {
        interpreter.obedientInterpret(bytes, notifications, connection, connections);
    }
);

interpreter.addObedientInterpretedMessageCallback(
    [](communication::Message message, std::uint16_t clientId) {
        const std::string_view text{
            reinterpret_cast<const char*>(message.payload.data()), message.payload.size()
        };
        std::println("Client {}: {}", clientId, text);
    }
);
```

### Start the server

Start listening on port `41333`, with an established connection timeout of `3000` ms, and reserve one slot for the batcher's pending message.

```cpp
if (const auto result = server.start(41333, 3000); !result) {
    std::println("Failed to start: {}", result.error().message());
    return 1;
}
network::initMessagesBatcher(1);
```

### Prepare an event

Convert the text to bytes and include the header and payload in the total size. For `Event`, the size is shifted one bit to the left. Type `990` is chosen by the application.

```cpp
const std::string_view greeting = "Hello, World!";
const auto payload = std::as_bytes(std::span{greeting});
const communication::Event event{
    // The least significant bit distinguishes Event from Response.
    static_cast<std::uint16_t>((sizeof(communication::Event) + payload.size()) << 1),
    990
};
```

### Update and send

`update()` processes the network. The converter creates an `OutboundMessage`, which is passed directly to `queueMessage`. `Target::broadcast()` selects the connections registered at flush time.

```cpp
while (true) {
    if (const auto result = server.update(); !result) {
        std::println("Failed to update: {}", result.error().message());
        break;
    }
    if (const auto result = network::queueMessage(communication::event2OutboundMessage(
            event, payload, network::Target::broadcast())); !result) {
        std::println("Failed to queue: {}", result.error().message());
        break;
    }
    if (const auto result = network::flushAll(connections); !result) {
        std::println("Failed to send: {}", result.error().message());
        break;
    }
    std::this_thread::sleep_for(3s);
}
```

`queueMessage` takes ownership of the message, including when it returns an error; the application does not call `Release()` afterward. `flushAll` processes the queue and releases the messages. The three-second interval is a choice made for this example; the application defines its own update rate.

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
client.addServerReceivedMessageCallback([&](std::span<const std::byte> bytes) {
    interpreter.authorityInterpret(bytes);
});

interpreter.addAuthorityInterpretedMessageCallback([](communication::Message message) {
    const auto text = message.payload.empty() ? std::string_view{} : std::string_view{
        reinterpret_cast<const char*>(message.payload.data()), message.payload.size()
    };
    std::println("Server: {}", text);
});
```

The callbacks use the bytes during the call. To retain a received payload, copy its bytes into application storage before returning.

### Start and connect

Initialize the client, request a connection to the server, and reserve space in the batcher. The connection will be tracked during calls to `update()`.

```cpp
if (const auto result = client.start(); !result) {
    std::println("Failed to start: {}", result.error().message());
    return 1;
}
if (const auto result = client.connect("127.0.0.1", 41333); !result) {
    std::println("Failed to connect: {}", result.error().message());
    return 1;
}
network::initMessagesBatcher(1);
```

### Prepare a notification

`Notification` uses the total size directly, without bit shifting. ID `47` corresponds to the registration made on the server.

```cpp
const std::string_view greeting = "Hello, World!";
const auto payload = std::as_bytes(std::span{greeting});
const communication::Notification notification{
    static_cast<std::uint16_t>(sizeof(communication::Notification) + payload.size()),
    47 // The server must register this ID in NotificationRegistry.
};
```

### Update and send to the server

Process up to 32 messages per update and send when `isConnected()` indicates an established connection. `Target::single` selects the server's handle.

```cpp
while (true) {
    if (const auto result = client.update(32); !result) {
        std::println("Failed to update: {}", result.error().message());
        break;
    }
    if (client.isConnected()) {
        if (const auto result = network::queueMessage(communication::notification2OutboundMessage(
                notification, payload, network::Target::single(client.getServerHConn()))); !result) {
            std::println("Failed to queue: {}", result.error().message());
            break;
        }
        if (const auto result = network::flushAll(connections); !result) {
            std::println("Failed to send: {}", result.error().message());
            break;
        }
    }
    std::this_thread::sleep_for(3000ms);
}
```

### Shut down the client

When exiting due to an error, the `Client` destructor closes the connection and shuts down the network. Returning `1` indicates the failure that interrupted the example.

```cpp
return 1; // The Client destructor closes the connection and shuts down the network.
```

## Complete code

The programs below bring together the snippets presented above. The text payload, IDs, loop, and intervals are choices made by the consuming application.

### Complete server

```cpp
#include <steam/steamnetworkingtypes.h>

import std;

import lightnerx.network.server;
import lightnerx.network.connection_manager;
import lightnerx.network.message_batch;
import lightnerx.network.target;
import lightnerx.communication.message;
import lightnerx.communication.notification;
import lightnerx.communication.event;

int main() {
    using namespace std::chrono_literals;
    namespace network = lightnerx::network;
    namespace communication = lightnerx::communication;

    network::Server server;
    network::ConnectionManager connections{30, 30};
    communication::MessageInterpreter interpreter;
    communication::NotificationRegistry notifications;
    notifications.registerId(47);

    server.addClientConnectedCallback([&](HSteamNetConnection connection) {
        if (!connections.add(connection)) {
            if (const auto result = server.disconnectConnection(connection, false); !result) {
                std::println("Failed to disconnect: {}", result.error().message());
            }
        }
    });

    server.addClientDisconnectedCallback([&](HSteamNetConnection connection) {
        if (const auto id = connections.getClientId(connection)) {
            connections.remove(*id);
        }
    });

    server.addClientReceivedMessageCallback(
        [&](std::span<const std::byte> bytes, HSteamNetConnection connection) {
            interpreter.obedientInterpret(bytes, notifications, connection, connections);
        }
    );

    interpreter.addObedientInterpretedMessageCallback(
        [](communication::Message message, std::uint16_t clientId) {
            const std::string_view text{
                reinterpret_cast<const char*>(message.payload.data()), message.payload.size()
            };
            std::println("Client {}: {}", clientId, text);
        }
    );

    if (const auto result = server.start(41333, 3000); !result) {
        std::println("Failed to start: {}", result.error().message());
        return 1;
    }
    network::initMessagesBatcher(1);

    const std::string_view greeting = "Hello, World!";
    const auto payload = std::as_bytes(std::span{greeting});
    const communication::Event event{
        // The least significant bit distinguishes Event from Response.
        static_cast<std::uint16_t>((sizeof(communication::Event) + payload.size()) << 1),
        990
    };

    while (true) {
        if (const auto result = server.update(); !result) {
            std::println("Failed to update: {}", result.error().message());
            break;
        }
        if (const auto result = network::queueMessage(communication::event2OutboundMessage(
                event, payload, network::Target::broadcast())); !result) {
            std::println("Failed to queue: {}", result.error().message());
            break;
        }
        if (const auto result = network::flushAll(connections); !result) {
            std::println("Failed to send: {}", result.error().message());
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
#include <steam/steamnetworkingtypes.h>

import std;

import lightnerx.network.client;
import lightnerx.network.connection_manager;
import lightnerx.network.message_batch;
import lightnerx.network.target;
import lightnerx.communication.message;
import lightnerx.communication.notification;

int main() {
    using namespace std::chrono_literals;
    namespace network = lightnerx::network;
    namespace communication = lightnerx::communication;

    network::Client client;
    network::ConnectionManager connections{0, 0}; // Required by flush, without broadcast.
    communication::MessageInterpreter interpreter;

    client.addServerReceivedMessageCallback([&](std::span<const std::byte> bytes) {
        interpreter.authorityInterpret(bytes);
    });

    interpreter.addAuthorityInterpretedMessageCallback([](communication::Message message) {
        const auto text = message.payload.empty() ? std::string_view{} : std::string_view{
            reinterpret_cast<const char*>(message.payload.data()), message.payload.size()
        };
        std::println("Server: {}", text);
    });

    if (const auto result = client.start(); !result) {
        std::println("Failed to start: {}", result.error().message());
        return 1;
    }
    if (const auto result = client.connect("127.0.0.1", 41333); !result) {
        std::println("Failed to connect: {}", result.error().message());
        return 1;
    }
    network::initMessagesBatcher(1);

    const std::string_view greeting = "Hello, World!";
    const auto payload = std::as_bytes(std::span{greeting});
    const communication::Notification notification{
        static_cast<std::uint16_t>(sizeof(communication::Notification) + payload.size()),
        47 // The server must register this ID in NotificationRegistry.
    };

    while (true) {
        if (const auto result = client.update(32); !result) {
            std::println("Failed to update: {}", result.error().message());
            break;
        }
        if (client.isConnected()) {
            if (const auto result = network::queueMessage(communication::notification2OutboundMessage(
                    notification, payload, network::Target::single(client.getServerHConn()))); !result) {
                std::println("Failed to queue: {}", result.error().message());
                break;
            }
            if (const auto result = network::flushAll(connections); !result) {
                std::println("Failed to send: {}", result.error().message());
                break;
            }
        }
        std::this_thread::sleep_for(3000ms);
    }

    return 1; // The Client destructor closes the connection and shuts down the network.
}
```

## Runtime contracts

One started `Client` or `Server` owns networking per process. Both classes have stable addresses and cannot be copied or moved. `Server::close()` and either owner's destructor release the runtime; a client can disconnect and reconnect without being destroyed. Direct `networkInit()` / `networkKill()` remain available when using the transport without a wrapper. See [API reference](docs/api.md) for shutdown, callback exceptions, reentrancy and message validation contracts.
