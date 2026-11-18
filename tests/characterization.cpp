#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingsockets.h>
#include <cstddef>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <lightnerx/compact/expected.hpp>
#include <iostream>
#include <tcb/span.hpp>
#include <stdexcept>
#include <system_error>
#include <type_traits>
#include <variant>
#include <vector>
#include <lightnerx/communication/message.hpp>
#include <lightnerx/communication/event.hpp>
#include <lightnerx/communication/intention.hpp>
#include <lightnerx/communication/notification.hpp>
#include <lightnerx/communication/response.hpp>
#include <lightnerx/network/target.hpp>
#include <lightnerx/network/message_batch.hpp>
#include <lightnerx/network/connection_manager.hpp>
#include <lightnerx/network/network_manager.hpp>
#include <lightnerx/network/client.hpp>
#include <lightnerx/network/server.hpp>
#include <lightnerx/network/transport.hpp>

namespace c = lightnerx::communication;
namespace n = lightnerx::network;

namespace {
std::uint64_t digest = 14695981039346656037ULL;
std::size_t delivered = 0;
std::size_t calls = 0;

void mix(std::uint64_t value) {
    for (unsigned i = 0; i < 8; ++i) {
        digest = (digest ^ (value & 255)) * 1099511628211ULL;
        value >>= 8;
    }
}

void record(c::Message message, std::uint16_t sender) {
    ++delivered;
    mix(sender);
    mix(message.header.index());
    std::visit([](const auto& header) {
        using T = std::decay_t<decltype(header)>;
        mix(header.protocol.size);
        if constexpr (std::is_same_v<T, c::Event> || std::is_same_v<T, c::Notification> ||
                      std::is_same_v<T, c::Intention>) mix(header.mbt.type);
        if constexpr (std::is_same_v<T, c::Response> || std::is_same_v<T, c::UnsuccessfulResponse> ||
                      std::is_same_v<T, c::Intention>) mix(header.correlationId);
        if constexpr (std::is_same_v<T, c::Response> || std::is_same_v<T, c::UnsuccessfulResponse>)
            mix(header.status);
        if constexpr (std::is_same_v<T, c::UnsuccessfulResponse>) mix(header.reasonCode);
    }, message.header);
    mix(message.payload.size());
    for (auto byte : message.payload) mix(std::to_integer<unsigned>(byte));
}

std::vector<std::byte> wire(const char* name, n::OutboundMessage outbound) {
    const auto* first = static_cast<const std::byte*>(outbound.message->m_pData);
    std::vector<std::byte> result(first, first + outbound.message->m_cbSize);
    outbound.message->Release();
    constexpr char digits[] = "0123456789abcdef";
    std::cout << name << '=';
    for (auto byte : result) {
        const auto value = std::to_integer<unsigned>(byte);
        std::cout << digits[value >> 4] << digits[value & 15];
    }
    std::cout << '\n';
    return result;
}

void error(const char* name, lightnerx::compact::expected<void, std::error_code> result) {
    std::cout << name << '=' << (result ? 0 : result.error().value()) << '\n';
}
}

int main() {
    try {
        static_assert(sizeof(c::Event) == 4 && sizeof(c::Notification) == 4);
        static_assert(sizeof(c::Intention) == 6 && sizeof(c::Response) == 5);
        static_assert(sizeof(c::UnsuccessfulResponse) == 7);
        static_assert(alignof(c::Event) == 1 && alignof(c::Intention) == 1);
        static_assert(alignof(c::Response) == 1 && alignof(c::UnsuccessfulResponse) == 1);
        static_assert(offsetof(c::Intention, correlationId) == 4);
        static_assert(offsetof(c::UnsuccessfulResponse, reasonCode) == 5);

        n::Client client;
        n::Server server;
        error("unstarted-client-update", client.update(1));
        error("unstarted-client-connect", client.connect("", 0));
        error("unstarted-server-update", server.update());
        error("invalid-server-port", server.start(0, 1000));
        error("invalid-server-timeout", server.start(45333, 0xffffffffU));
        error("uninitialized-send", n::sendToConnection({}, 0));
        if (!n::networkInit()) throw std::runtime_error("network initialization");
        HSteamNetConnection sender{}, peer{};
        if (!SteamNetworkingSockets()->CreateSocketPair(&sender, &peer, false, nullptr, nullptr))
            throw std::runtime_error("socket pair");
        n::ConnectionManager connections(0, 2);
        if (connections.add(sender) != 1) throw std::runtime_error("sender registration");
        error("invalid-connection", n::sendToConnection({}, 0));
        error("empty-group", n::sendToConnections({}, {}));
        error("empty-flush", n::flushAll(connections));
        error("null-queue", n::queueMessage({nullptr, n::Target::single(sender)}));

        const std::array payload{std::byte{0}, std::byte{0x7f}, std::byte{0xff}};
        const auto target = n::Target::single(sender);
        std::vector<std::vector<std::byte>> frames;
        frames.push_back(wire("event", c::event2OutboundMessage(c::Event(14, 0x1234), payload, target)));
        frames.push_back(wire("intention", c::intention2OutboundMessage(c::Intention(9, 48, 0xabcd), payload, target)));
        frames.push_back(wire("notification", c::notification2OutboundMessage(c::Notification(7, 47), payload, target)));
        frames.push_back(wire("approved-extra", c::response2OutboundMessage(c::Response(17, 0xabcd, 0), payload, target)));
        frames.push_back(wire("denied-extra", c::unsuccessfulResponse2OutboundMessage(c::UnsuccessfulResponse(21, 0xabcd, 1, 0x4321), payload, target)));
        frames.push_back(wire("partial", c::unsuccessfulResponse2OutboundMessage(c::UnsuccessfulResponse(21, 0xabcd, 2, 0x4321), payload, target)));
        frames.push_back(wire("failure-extra", c::response2OutboundMessage(c::Response(17, 0xabcd, 3), payload, target)));

        c::NotificationRegistry registry;
        registry.registerIds({47, 0xffff});
        c::MessageInterpreter interpreter;
        interpreter.addAuthorityInterpretedMessageCallback([](c::Message message) { record(message, 0); });
        interpreter.addObedientInterpretedMessageCallback(record);
        auto interpret = [&](tcb::span<const std::byte> bytes) {
            mix(++calls);
            interpreter.authorityInterpret(bytes);
            mix(++calls);
            interpreter.obedientInterpret(bytes, registry, sender, connections);
            mix(++calls);
            interpreter.obedientInterpret(bytes, registry, 0, connections);
        };
        for (auto frame : frames) {
            for (std::size_t length = 0; length <= frame.size(); ++length)
                interpret(tcb::span{frame}.first(length));
            frame.push_back(std::byte{});
            interpret(frame);
        }
        // Exhaust every response status, including tolerated extra payload bytes.
        for (unsigned status = 0; status < 256; ++status) {
            auto frame = frames[4];
            frame[4] = static_cast<std::byte>(status);
            interpret(frame);
        }
        std::uint32_t random = 0x4c58;
        auto next = [&] {
            random ^= random << 13;
            random ^= random >> 17;
            random ^= random << 5;
            return random;
        };
        for (unsigned sample = 0; sample < 4096; ++sample) {
            std::vector<std::byte> frame(next() % 65);
            for (auto& byte : frame) byte = static_cast<std::byte>(next() & 255);
            if (frame.size() >= 2 && sample % 3 != 0) {
                const auto size = static_cast<std::uint16_t>(sample % 3 == 1
                    ? frame.size() : ((frame.size() << 1) | (sample & 1)));
                std::memcpy(frame.data(), &size, sizeof(size));
            }
            if (frame.size() >= 4 && sample % 5 == 0) {
                const std::uint16_t type = 47;
                std::memcpy(frame.data() + 2, &type, sizeof(type));
            }
            interpret(frame);
        }
        std::cout << "parser-calls=" << calls << "\ndelivered=" << delivered << "\ndigest=" << digest << '\n';
        connections.remove(1);
        SteamNetworkingSockets()->CloseConnection(sender, 0, nullptr, false);
        SteamNetworkingSockets()->CloseConnection(peer, 0, nullptr, false);
        n::networkKill();
    } catch (const std::exception& failure) {
        std::cerr << failure.what() << '\n';
        n::networkKill();
        return 1;
    }
}
