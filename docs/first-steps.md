# First steps

The examples use `std::string_view` for byte views and `lightnerx::Result` for results. See [C++17 byte views and results](cpp17-types.md).

This guide shows the essential flow for setting up a server and a client. The examples use LightnerX components directly and leave the execution model to the application. Each program is built as a separate application.

## Server

The server receives bytes from clients, forwards those bytes to `MessageInterpreter`, and receives interpreted messages through a callback.

```cpp

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <thread>
#include <type_traits>
#include <variant>
#include <string_view>

#include <lightnerx/communication/intention.hpp>
#include <lightnerx/communication/message.hpp>
#include <lightnerx/communication/notification.hpp>
#include <lightnerx/network/connection_manager.hpp>
#include <lightnerx/network/server.hpp>

int main() {
    lightnerx::network::Server server;
    lightnerx::network::ConnectionManager connections{10, 10};
    lightnerx::communication::MessageInterpreter interpreter;
    lightnerx::communication::NotificationRegistry registry;

    registry.registerId(47);

    server.addClientConnectedCallback(
        [&connections, &server](lightnerx::network::ConnectionHandle connection) {
            if (!connections.add(connection)) {
                const auto disconnected = server.disconnectConnection(connection, false);
                if (!disconnected) {
                    std::cout << "Failed to disconnect: " << lightnerx::errorMessage(disconnected.error()) << '\n';
                }
            }
        }
    );

    server.addClientDisconnectedCallback(
        [&connections](lightnerx::network::ConnectionHandle connection) {
            if (const auto id = connections.getClientId(connection)) {
                connections.remove(*id);
            }
        }
    );

    server.addClientReceivedMessageCallback(
        [&interpreter, &registry, &connections](
            std::string_view bytes,
            lightnerx::network::ConnectionHandle connection
        ) {
            interpreter.obedientInterpret(
                bytes,
                registry,
                connection,
                connections
            );
        }
    );

    interpreter.addObedientInterpretedMessageCallback(
        [](lightnerx::communication::Message message, std::uint16_t clientId) {
            std::cout << "Message received from client " << clientId << '\n';
            std::visit([](const auto& header) {
                using Header = std::remove_cv_t<std::remove_reference_t<decltype(header)>>;
                if constexpr (std::is_same_v<Header, lightnerx::communication::Intention>) {
                    std::cout << "Intention " << header.mbt.type << ", correlation " << header.correlationId << '\n';
                } else if constexpr (std::is_same_v<Header, lightnerx::communication::Notification>) {
                    std::cout << "Notification " << header.mbt.type << '\n';
                }
            }, message.header);
        }
    );

    const auto started = server.start(45333, 2500);
    if (!started) {
        std::cout << "Failed to start: " << lightnerx::errorMessage(started.error()) << '\n';
        return 1;
    }

    while (true) {
        if (const auto updated = server.update(); !updated) {
            std::cout << "Update failed: " << lightnerx::errorMessage(updated.error()) << '\n';
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }

    server.close();
    return 1;
}
```

The connection callback must update the `ConnectionManager`, because the interpreter uses this object to map the connection to the ID delivered to the interpreted message callback.

The lambdas capture the components by reference to connect them. Keep these objects alive as long as their callbacks may run. The received `payload` points to the network message's bytes; copy the bytes during the callback if you need to use them later.

## Client

The client forwards bytes received from the server to the `authorityInterpret` method.

```cpp

#include <chrono>
#include <cstddef>
#include <iostream>
#include <thread>
#include <type_traits>
#include <variant>
#include <string_view>

#include <lightnerx/communication/event.hpp>
#include <lightnerx/communication/message.hpp>
#include <lightnerx/communication/response.hpp>
#include <lightnerx/network/client.hpp>
#include <lightnerx/network/connection_handle.hpp>

int main() {
    lightnerx::network::Client client;
    lightnerx::communication::MessageInterpreter interpreter;

    client.addServerReceivedMessageCallback(
        [&interpreter](std::string_view bytes) {
            interpreter.authorityInterpret(bytes);
        }
    );

    interpreter.addAuthorityInterpretedMessageCallback(
        [](lightnerx::communication::Message message) {
            std::visit([](const auto& header) {
                using Header = std::remove_cv_t<std::remove_reference_t<decltype(header)>>;
                if constexpr (std::is_same_v<Header, lightnerx::communication::Event>) {
                    std::cout << "Event " << header.mbt.type << '\n';
                } else if constexpr (
                    std::is_same_v<Header, lightnerx::communication::Response> ||
                    std::is_same_v<Header, lightnerx::communication::UnsuccessfulResponse>
                ) {
                    std::cout << "Response to intention " << header.correlationId << ": status " << static_cast<unsigned>(header.status) << '\n';
                    if constexpr (std::is_same_v<Header, lightnerx::communication::UnsuccessfulResponse>) {
                        std::cout << "Reason " << header.reasonCode << '\n';
                    }
                }
            }, message.header);
        }
    );

    if (const auto started = client.start(); !started) {
        std::cout << "Failed to start: " << lightnerx::errorMessage(started.error()) << '\n';
        return 1;
    }

    if (const auto connected = client.connect("127.0.0.1", 45333);
        !connected) {
        std::cout << "Failed to connect: " << lightnerx::errorMessage(connected.error()) << '\n';
        return 1;
    }

    const auto connectDeadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
    bool connectedOnce = false;
    while (true) {
        if (const auto updated = client.update(1); !updated) {
            std::cout << "Update failed: " << lightnerx::errorMessage(updated.error()) << '\n';
            return 1;
        }
        if (client.getServerHConn() == lightnerx::network::k_invalidConnection) {
            std::cout << (connectedOnce ? "Connection closed\n" : "Connection attempt failed\n");
            return connectedOnce ? 0 : 1;
        }
        connectedOnce = connectedOnce || client.isConnected();
        if (!connectedOnce && std::chrono::steady_clock::now() >= connectDeadline) {
            std::cout << "Connection attempt timed out\n";
            return 1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
}
```

`connect()` starts the connection attempt. The client calls `update()` until it observes an established connection, a closed handle, or the application's five-second deadline. An update that processes a peer closure can clear the handle and return success. The loop therefore checks `getServerHConn()` after every update. In this example, the end of an established session exits with status 0. A failed attempt or update exits with status 1. The client destructor releases the runtime on exit.

## Execution model

The `server.update()` and `client.update()` methods are called by the application. They can run on the main thread, on a dedicated thread, or within the consuming project's existing execution loop.

The application chooses the number of threads and the interval between updates. Run these server and client examples in separate processes so each wrapper can own its LightnerX runtime. Keep wrappers at a fixed address for their lifetime.

Receive callbacks run during `update()`, and interpretation callbacks run on the thread that calls the interpreter. When sharing components across threads, handle synchronization in the application. For the batcher, serialize initialization, queuing, clearing and flush calls, which operate on the same queue. Callbacks invoked during transport connection status dispatch must complete without throwing. See [callback behavior](api.md#validation-and-callback-behavior) for supported reentrant operations.

## Batch sending

With the network initialized by `Server::start()`, `Client::start()`, or `lightnerx::network::networkInit()`, reserve space in the batcher before queuing the first message. Include `<lightnerx/network/message_batch.hpp>` and call:

```cpp
if (const auto reserved = lightnerx::network::initMessagesBatcher(64); !reserved) {
    return 1;
}
```

If a reservation fails during allocation, `std::bad_alloc` can propagate before the result check. See the [batcher reference](api.md#batcher).

The destination is defined by `lightnerx::network::Target`. The following function creates an `Event`, routes it to a group, and sends the queue. It assumes the initialization above and returns the result of queuing or flushing to the caller. The same flow applies to the other message converters.

```cpp

#include <cstddef>
#include <cstdint>
#include <utility>
#include <string_view>
#include <vector>
#include <lightnerx/Result.hpp>

#include <lightnerx/communication/event.hpp>
#include <lightnerx/communication/message.hpp>
#include <lightnerx/network/connection_manager.hpp>
#include <lightnerx/network/message_batch.hpp>
#include <lightnerx/network/target.hpp>

lightnerx::Result<void> sendEventToGroup(
    std::string_view payload,
    std::vector<lightnerx::network::ConnectionHandle> receivers,
    const lightnerx::network::ConnectionManager& connections
) {
    if (payload.size() > 0x7fff - sizeof(lightnerx::communication::Event)) {
        return lightnerx::Result<void>::failure(lightnerx::ErrorCode::MessageSize);
    }
    const auto totalSize = sizeof(lightnerx::communication::Event) + payload.size();

    const lightnerx::communication::Event event{
        static_cast<std::uint16_t>(totalSize << 1),
        47
    };

    auto outbound = lightnerx::communication::event2OutboundMessage(
        event,
        payload,
        lightnerx::network::Target::group(std::move(receivers))
    );

    const auto queued = lightnerx::network::queueMessage(std::move(outbound));

    if (!queued) {
        return queued;
    }

    return lightnerx::network::flushAll(connections);
}
```

`queueMessage` receives the `OutboundMessage` created by the converter and takes ownership of the message and its destination, without reallocating the message or copying its bytes. If the queue is full, the call releases the message immediately and returns an error. If queuing is accepted, `flushAll` releases the message after the send attempt.

Pass an existing result with `std::move(outbound)` or pass the converter's result directly to `queueMessage`. The converter copies the bytes. A result holding an unqueued message releases that message when destroyed.

With handles obtained by the client or through the server's connection callbacks, the available destinations are:

```cpp
auto oneClient = lightnerx::network::Target::single(connection);
auto selectedClients = lightnerx::network::Target::group({firstConnection, secondConnection});
auto everyone = lightnerx::network::Target::broadcast();
auto everyoneElse = lightnerx::network::Target::broadcast({connectionToSkip});
```

These calls only describe destinations. You can queue several messages and run `flushAll(connections)` at the end of an application step; the immediate flush shown in the function is a choice made for the example.

## Next steps

- See [Communication and the INER protocol](communication.md) to understand headers, destinations, and sending flows.
- See [API reference](api.md) for networking components, messages, `Target`, and the batcher.
