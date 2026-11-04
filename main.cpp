#include "steam/steamnetworkingtypes.h"

#include <atomic>
#include <bit>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <vector>
#include <variant>

import network.server;
import network.client;
import network.connection_manager;
import network.message_batch;
import communication.event;
import communication.intention;
import communication.message;
import communication.notification;
import communication.response;

namespace {
constexpr int kServerPort = 45333;
constexpr int kUpdateIntervalMs = 40;
constexpr int kMessageBatchCapacity = 4096;

void clearTerminal() {
    const int clearResult = std::system("clear");
    if (clearResult == -1) {
        return;
    }
}

std::optional<unsigned int> readNumber(
    std::string_view screen,
    std::string_view prompt,
    unsigned int minimum,
    unsigned int maximum
) {
    while (true) {
        clearTerminal();
        std::println("{}", screen);
        std::print("{}", prompt);
        std::fflush(stdout);

        std::string line;
        if (!std::getline(std::cin, line)) {
            return std::nullopt;
        }

        unsigned int value = 0;
        const auto [end, error] = std::from_chars(
            line.data(), line.data() + line.size(), value
        );
        if (error == std::errc{} && end == line.data() + line.size()
            && value >= minimum && value <= maximum) {
            return value;
        }
    }
}

std::optional<std::vector<std::byte>> readPayload(std::string_view screen) {
    while (true) {
        clearTerminal();
        std::println("{}", screen);
        std::print("Payload em texto: ");
        std::fflush(stdout);

        std::string text;
        if (!std::getline(std::cin, text)) {
            return std::nullopt;
        }

        if (!text.empty()) {
            std::vector<std::byte> payload(text.size());
            std::memcpy(payload.data(), text.data(), text.size());
            return payload;
        }
    }
}

std::optional<uint16_t> readUint16(
    std::string_view screen,
    std::string_view prompt,
    uint16_t minimum = 0
) {
    const auto value = readNumber(screen, prompt, minimum, std::numeric_limits<uint16_t>::max());
    if (!value) {
        return std::nullopt;
    }
    return static_cast<uint16_t>(*value);
}

std::optional<uint16_t> encodedMessageSize(std::size_t totalSize, bool response) {
    constexpr auto maxSize = static_cast<std::size_t>(std::numeric_limits<uint16_t>::max() >> 1);
    if (totalSize > maxSize) {
        return std::nullopt;
    }
    return static_cast<uint16_t>((totalSize << 1) | (response ? 1 : 0));
}

std::optional<uint16_t> plainMessageSize(std::size_t totalSize) {
    if (totalSize > std::numeric_limits<uint16_t>::max()) {
        return std::nullopt;
    }
    return static_cast<uint16_t>(totalSize);
}

bool queueAndFlush(network::OutboundMessage outbound) {
    const std::span<const std::byte> bytes{
        static_cast<const std::byte*>(outbound.message->m_pData),
        static_cast<std::size_t>(outbound.message->m_cbSize)
    };

    const auto queued = network::queueMessage(bytes, outbound.connection);
    outbound.message->Release();
    if (!queued) {
        std::println(stderr, "Nao foi possivel enfileirar a mensagem: {}", queued.error().message());
        return false;
    }

    const auto flushed = network::flushAll();
    if (!flushed) {
        std::println(stderr, "Nao foi possivel enviar a mensagem: {}", flushed.error().message());
        return false;
    }
    return true;
}

bool sendEvent(HSteamNetConnection connection, uint16_t typeId, const std::vector<std::byte>& payload) {
    const auto size = encodedMessageSize(sizeof(communication::Event) + payload.size(), false);
    if (!size) {
        std::println(stderr, "Payload grande demais.");
        return false;
    }

    const communication::Event event{*size, typeId};
    return queueAndFlush(communication::event2OutboundMessage(
        event, {payload.data(), payload.size()}, connection
    ));
}

bool sendResponse(
    HSteamNetConnection connection,
    uint16_t correlationId,
    uint8_t status,
    std::optional<uint16_t> reasonCode,
    const std::vector<std::byte>& payload
) {
    if (status == communication::APPROVED_RESPONSE || status == communication::FAILURE_RESPONSE) {
        const auto size = encodedMessageSize(sizeof(communication::Response), true);
        if (!size) {
            return false;
        }
        const communication::Response response{*size, correlationId, status};
        return queueAndFlush(communication::response2OutboundMessage(response, {}, connection));
    }

    const auto size = encodedMessageSize(
        sizeof(communication::UnsuccessfulResponse) + payload.size(), true
    );
    if (!size || !reasonCode) {
        std::println(stderr, "Resposta invalida.");
        return false;
    }

    const communication::UnsuccessfulResponse response{*size, correlationId, status, *reasonCode};
    return queueAndFlush(communication::unsuccessfulResponse2OutboundMessage(
        response, {payload.data(), payload.size()}, connection
    ));
}

bool sendNotification(HSteamNetConnection connection, uint16_t typeId, const std::vector<std::byte>& payload) {
    const auto size = plainMessageSize(sizeof(communication::Notification) + payload.size());
    if (!size) {
        std::println(stderr, "Payload grande demais.");
        return false;
    }

    const communication::Notification notification{*size, typeId};
    static_assert(sizeof(communication::Notification) == sizeof(communication::Event));

    // Notification e Event possuem o mesmo layout no protocolo. O conversor de
    // Notification ainda nao existe em communication.message, entao reutilizamos
    // o conversor de OutboundMessage com os bytes do cabecalho Notification.
    const auto wireHeader = std::bit_cast<communication::Event>(notification);
    return queueAndFlush(communication::event2OutboundMessage(
        wireHeader, {payload.data(), payload.size()}, connection
    ));
}

bool sendIntention(
    HSteamNetConnection connection,
    uint16_t typeId,
    uint16_t correlationId,
    const std::vector<std::byte>& payload
) {
    const auto size = plainMessageSize(sizeof(communication::Intention) + payload.size());
    if (!size) {
        std::println(stderr, "Payload grande demais.");
        return false;
    }

    const communication::Intention intention{*size, typeId, correlationId};
    return queueAndFlush(communication::intention2OutboundMessage(
        intention, {payload.data(), payload.size()}, connection
    ));
}

bool sendServerMessage(HSteamNetConnection connection) {
    const auto kind = readNumber(
        "Envio de mensagem\n\n0 - Event\n1 - Response\n2 - Voltar/Sair",
        "Tipo: ", 0, 2
    );
    if (!kind || *kind == 2) {
        return false;
    }

    if (*kind == 0) {
        const auto typeId = readUint16("Event", "Type ID: ");
        if (!typeId) {
            return false;
        }
        const auto payload = readPayload("Event");
        if (!payload) {
            return false;
        }
        sendEvent(connection, *typeId, *payload);
        return true;
    }

    const auto status = readNumber(
        "Response\n\n0 - Approved\n1 - Denied\n2 - PartiallyApproved\n3 - Failed",
        "Status: ", communication::APPROVED_RESPONSE, communication::FAILURE_RESPONSE
    );
    if (!status) {
        return false;
    }

    const auto correlationId = readUint16("Response", "Correlation ID: ");
    if (!correlationId) {
        return false;
    }

    const auto responseStatus = static_cast<uint8_t>(*status);
    std::optional<uint16_t> reasonCode;
    std::vector<std::byte> payload;

    if (responseStatus == communication::DENIED_RESPONSE
        || responseStatus == communication::PARTIALLY_APPROVED_RESPONSE) {
        reasonCode = readUint16("Response", "Reason code: ");
        if (!reasonCode) {
            return false;
        }
    }

    if (responseStatus == communication::PARTIALLY_APPROVED_RESPONSE) {
        const auto enteredPayload = readPayload("Response parcialmente aprovada");
        if (!enteredPayload) {
            return false;
        }
        payload = *enteredPayload;
    }

    sendResponse(connection, *correlationId, responseStatus, reasonCode, payload);
    return true;
}

std::optional<uint16_t> readNotificationType() {
    while (true) {
        const auto typeId = readUint16(
            "Notification\n\nIDs registrados: 47 e 99.",
            "Type ID: "
        );
        if (!typeId || *typeId == 47 || *typeId == 99) {
            return typeId;
        }
    }
}

bool sendClientMessage(HSteamNetConnection connection) {
    const auto kind = readNumber(
        "Envio de mensagem\n\n0 - Notification\n1 - Intention\n2 - Sair",
        "Tipo: ", 0, 2
    );
    if (!kind || *kind == 2) {
        return false;
    }

    if (*kind == 0) {
        const auto typeId = readNotificationType();
        if (!typeId) {
            return false;
        }
        const auto payload = readPayload("Notification");
        if (!payload) {
            return false;
        }
        sendNotification(connection, *typeId, *payload);
        return true;
    }

    const auto typeId = readUint16("Intention", "Type ID: ");
    if (!typeId) {
        return false;
    }
    const auto correlationId = readUint16("Intention", "Correlation ID: ");
    if (!correlationId) {
        return false;
    }
    const auto payload = readPayload("Intention");
    if (!payload) {
        return false;
    }

    sendIntention(connection, *typeId, *correlationId, *payload);
    return true;
}

std::string payloadAsText(std::span<const std::byte> payload) {
    if (payload.empty()) {
        return "(vazio)";
    }
    return {
        reinterpret_cast<const char*>(payload.data()),
        payload.size()
    };
}

void printReceivedMessage(const communication::Message& message, std::string_view sender) {
    std::println("Mensagem recebida de {}:", sender);

    std::size_t headerSize = 0;
    std::visit([&headerSize](const auto& header) {
        using Header = std::decay_t<decltype(header)>;
        headerSize = sizeof(Header);

        if constexpr (std::is_same_v<Header, communication::Event>) {
            std::println("Tipo: Event (type ID: {})", header.mbt.type);
        } else if constexpr (std::is_same_v<Header, communication::Notification>) {
            std::println("Tipo: Notification (type ID: {})", header.mbt.type);
        } else if constexpr (std::is_same_v<Header, communication::Intention>) {
            std::println(
                "Tipo: Intention (type ID: {}, correlation ID: {})",
                header.mbt.type,
                header.correlationId
            );
        } else if constexpr (std::is_same_v<Header, communication::Response>) {
            std::println(
                "Tipo: Response (status: {}, correlation ID: {})",
                static_cast<unsigned int>(header.status),
                header.correlationId
            );
        } else {
            std::println(
                "Tipo: UnsuccessfulResponse (status: {}, correlation ID: {}, reason code: {})",
                static_cast<unsigned int>(header.status),
                header.correlationId,
                header.reasonCode
            );
        }
    }, message.header);

    std::println("Tamanho recebido: {} bytes", headerSize + message.payload.size());
    std::println("Payload ({} bytes): {}", message.payload.size(), payloadAsText(message.payload));
}

void runServer() {
    network::Server server;
    network::ConnectionManager connManager{10, 10};
    communication::MessageInterpreter interpreter;
    communication::NotificationRegistry registry;

    registry.registerId(47);
    registry.registerId(99);

    server.addClientConnectedCallback([&connManager](HSteamNetConnection connection) {
        std::println("Cliente conectado.");
        connManager.add(connection);
    });
    server.addClientDisconnectedCallback([&connManager](HSteamNetConnection connection) {
        std::println("Cliente desconectado.");
        if (const auto id = connManager.getClientId(connection)) {
            connManager.remove(*id);
        }
    });
    server.addClientReceivedMessageCallback(
        [&connManager, &interpreter, &registry](std::span<const std::byte> message, HSteamNetConnection connection) {
            interpreter.obedientInterpret(message, registry, connection, connManager);
        }
    );
    interpreter.addObedientInterpretedMessageCallback([](communication::Message message, uint16_t id) {
        printReceivedMessage(message, "cliente " + std::to_string(id));
    });

    const auto started = server.start(kServerPort, 2500);
    if (!started) {
        std::println(stderr, "Falha ao iniciar o servidor: {}", started.error().message());
        return;
    }
    network::initMessagesBatcher(kMessageBatchCapacity);

    std::atomic_bool updating = true;
    std::thread updateThread([&server, &updating] {
        while (updating.load()) {
            server.update();
            std::this_thread::sleep_for(std::chrono::milliseconds(kUpdateIntervalMs));
        }
    });

    while (true) {
        const auto clientId = readUint16(
            "Envio pelo servidor\n\nInforme o ID do cliente conectado.",
            "Client ID: ", 1
        );
        if (!clientId) {
            break;
        }

        while (sendServerMessage(connManager.getHSteamNetConnection(*clientId))) {
        }

        if (!std::cin) {
            break;
        }
    }

    updating = false;
    updateThread.join();
    server.close();
}

void runClient() {
    network::Client client;
    communication::MessageInterpreter interpreter;

    client.addServerReceivedMessageCallback([&interpreter](std::span<const std::byte> message) {
        interpreter.authorityInterpret(message);
    });
    interpreter.addAuthorityInterpretedMessageCallback([](communication::Message message) {
        printReceivedMessage(message, "servidor");
    });

    const auto started = client.start();
    if (!started) {
        std::println(stderr, "Falha ao iniciar o cliente: {}", started.error().message());
        return;
    }
    const auto connected = client.connect("127.0.0.1", kServerPort);
    if (!connected) {
        std::println(stderr, "Falha ao conectar ao servidor: {}", connected.error().message());
        return;
    }
    network::initMessagesBatcher(kMessageBatchCapacity);

    std::atomic_bool updating = true;
    std::thread updateThread([&client, &updating] {
        while (updating.load()) {
            client.update(1);
            std::this_thread::sleep_for(std::chrono::milliseconds(kUpdateIntervalMs));
        }
    });

    while (sendClientMessage(client.getServerHConn())) {
    }

    updating = false;
    updateThread.join();
}
} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        return 1;
    }

    const std::string_view mode = argv[1];
    if (mode == "--server") {
        runServer();
        return 0;
    }
    if (mode == "--client") {
        runClient();
        return 0;
    }
    return 1;
}
