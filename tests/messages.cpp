#ifndef LIGHTNERX_SOURCE_TEST
#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <functional>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>
#include <lightnerx/communication/message.hpp>
#include <lightnerx/communication/event.hpp>
#include <lightnerx/communication/notification.hpp>
#include <lightnerx/communication/intention.hpp>
#include <lightnerx/communication/response.hpp>
#include <lightnerx/network/target.hpp>
#include <lightnerx/network/message_batch.hpp>
#include <lightnerx/network/connection_manager.hpp>
#include <lightnerx/network/network_manager.hpp>
#endif

using namespace lightnerx::communication;
using namespace lightnerx::network;

void require(bool condition, const char* description) {
    if (!condition) throw std::runtime_error(description);
}

std::string bytes(lightnerx::Result<OutboundMessage> outbound) {
    require(outbound.has_value(), "outbound conversion failed");
    return std::string(outbound.value().data());
}

void setSize(std::string& buffer, std::uint16_t size) {
    std::memcpy(buffer.data(), &size, sizeof(size));
}

int main() {
    try {
#ifndef LIGHTNERX_SOURCE_TEST
        require(networkInit(), "network initialization failed");
        HSteamNetConnection sender{}, peer{};
        require(SteamNetworkingSockets()->CreateSocketPair(&sender, &peer, false, nullptr, nullptr), "socket pair failed");
#else
        HSteamNetConnection sender = 1;
#endif
        ConnectionManager manager(2, 2);
        require(manager.add(sender).has_value(), "connection registration failed");
        NotificationRegistry registry;
        registry.registerId(47);
        const std::string_view payload{"\x2a\x00\xff", 3};
        const auto target = Target::single(sender);
        require(target.isSingle() && !target.isGroup() && !target.isBroadcast(), "single target");
        require(Target::group({sender}).isGroup(), "group target");
        require(Target::broadcast().isBroadcast(), "broadcast target");
        auto event = bytes(event2OutboundMessage(Event(14, 10), payload, target));
        require(std::string_view(event).substr(sizeof(Event)) == payload, "binary payload was altered");
        auto response = bytes(response2OutboundMessage(Response(11, 20, APPROVED_RESPONSE), {}, target));
        auto failure = bytes(response2OutboundMessage(Response(11, 20, FAILURE_RESPONSE), {}, target));
        auto denied = bytes(unsuccessfulResponse2OutboundMessage(UnsuccessfulResponse(15, 20, DENIED_RESPONSE, 9), {}, target));
        auto unsuccessful = bytes(unsuccessfulResponse2OutboundMessage(UnsuccessfulResponse(21, 20, PARTIALLY_APPROVED_RESPONSE, 9), payload, target));
        auto notification = bytes(notification2OutboundMessage(Notification(7, 47), payload, target));
        auto intention = bytes(intention2OutboundMessage(Intention(9, 48, 20), payload, target));
        MessageInterpreter interpreter;
        int authorityCount = 0, obedientCount = 0;
        interpreter.addAuthorityInterpretedMessageCallback([&](Message message) {
            ++authorityCount;
            std::visit([&](const auto& header) {
                require(header.protocol.size == sizeof(header) + message.payload.size(), "decoded authority size");
            }, message.header);
        });
        interpreter.addObedientInterpretedMessageCallback([&](Message message, std::uint16_t id) {
            require(id == *manager.getClientId(sender), "sender identity");
            ++obedientCount;
            std::visit([&](const auto& header) {
                require(header.protocol.size == sizeof(header) + message.payload.size(), "decoded obedient size");
            }, message.header);
        });
        for (const auto& valid : {event, response, failure, denied, unsuccessful}) {
            const auto before = authorityCount;
            interpreter.authorityInterpret(valid);
            require(authorityCount == before + 1, "valid authority message rejected");
            for (std::size_t size = 0; size < valid.size(); ++size)
                interpreter.authorityInterpret(std::string_view(valid).substr(0, size));
            auto bad = valid;
            setSize(bad, static_cast<unsigned char>(valid[0]) & 1u);
            interpreter.authorityInterpret(bad);
            setSize(bad, static_cast<std::uint16_t>(65534 | (static_cast<unsigned char>(valid[0]) & 1u)));
            interpreter.authorityInterpret(bad);
            bad = valid;
            bad.push_back('\0');
            interpreter.authorityInterpret(bad);
            require(authorityCount == before + 1, "malformed authority message accepted");
        }
        for (const auto& valid : {notification, intention}) {
            const auto before = obedientCount;
            interpreter.obedientInterpret(valid, registry, sender, manager);
            require(obedientCount == before + 1, "valid obedient message rejected");
            for (std::size_t size = 0; size < valid.size(); ++size)
                interpreter.obedientInterpret(std::string_view(valid).substr(0, size), registry, sender, manager);
            auto bad = valid;
            setSize(bad, 0);
            interpreter.obedientInterpret(bad, registry, sender, manager);
            setSize(bad, 65535);
            interpreter.obedientInterpret(bad, registry, sender, manager);
            bad = valid;
            bad.append(valid);
            interpreter.obedientInterpret(bad, registry, sender, manager);
            interpreter.obedientInterpret(valid, registry, 0, manager);
            require(obedientCount == before + 1, "malformed or unidentified obedient message accepted");
        }
        auto unknownStatus = response;
        unknownStatus[4] = static_cast<char>(255);
        const auto beforeUnknown = authorityCount;
        interpreter.authorityInterpret(unknownStatus);
        require(authorityCount == beforeUnknown, "unknown status accepted");
        // A self-consistent length still must contain the full selected header.
        auto shortResponse = response;
        shortResponse.resize(4);
        setSize(shortResponse, 9);
        interpreter.authorityInterpret(shortResponse);
        auto shortUnsuccessful = unsuccessful;
        shortUnsuccessful.resize(5);
        setSize(shortUnsuccessful, 11);
        interpreter.authorityInterpret(shortUnsuccessful);
        require(authorityCount == beforeUnknown, "short response header accepted");
        auto shortIntention = intention;
        shortIntention.resize(4);
        setSize(shortIntention, 4);
        const auto beforeShort = obedientCount;
        interpreter.obedientInterpret(shortIntention, registry, sender, manager);
        require(obedientCount == beforeShort, "short intention header accepted");

        // Exercise the largest representable complete messages and overflow-by-one buffers.
        const std::string authorityPayload(32767 - sizeof(Event), '\0');
        auto largestEvent = bytes(event2OutboundMessage(Event(65534, 10), authorityPayload, target));
        const auto beforeBoundaryAuthority = authorityCount;
        interpreter.authorityInterpret(largestEvent);
        largestEvent.push_back('\0');
        interpreter.authorityInterpret(largestEvent);
        require(authorityCount == beforeBoundaryAuthority + 1, "authority length boundary");
        const std::string obedientPayload(65535 - sizeof(Notification), '\0');
        auto largestNotification = bytes(notification2OutboundMessage(Notification(65535, 47), obedientPayload, target));
        const auto beforeBoundaryObedient = obedientCount;
        interpreter.obedientInterpret(largestNotification, registry, sender, manager);
        largestNotification.push_back('\0');
        interpreter.obedientInterpret(largestNotification, registry, sender, manager);
        require(obedientCount == beforeBoundaryObedient + 1, "obedient length boundary");

        MessageInterpreter reentrant;
        int lateAuthority = 0, lateObedient = 0;
        std::vector<int> callbackState;
        reentrant.addAuthorityInterpretedMessageCallback([&, state = 0](Message) mutable {
            callbackState.push_back(++state);
            if (state == 1) for (int i = 0; i < 1000; ++i)
                reentrant.addAuthorityInterpretedMessageCallback([&](Message) { ++lateAuthority; });
        });
        reentrant.addObedientInterpretedMessageCallback([&, state = 0](Message, std::uint16_t) mutable {
            if (++state == 1) for (int i = 0; i < 1000; ++i)
                reentrant.addObedientInterpretedMessageCallback([&](Message, std::uint16_t) { ++lateObedient; });
        });
        reentrant.authorityInterpret(event);
        reentrant.obedientInterpret(notification, registry, sender, manager);
        require(lateAuthority == 0 && lateObedient == 0, "new callback ran during same dispatch");
        reentrant.authorityInterpret(event);
        reentrant.obedientInterpret(notification, registry, sender, manager);
        require(lateAuthority == 1000 && lateObedient == 1000, "registered callbacks not retained");
        require(callbackState == std::vector<int>{1, 2}, "callback state was copied");
        reentrant.addAuthorityInterpretedMessageCallback([](Message) { throw 42; });
        bool caught = false;
        try { reentrant.authorityInterpret(event); } catch (int value) { caught = value == 42; }
        require(caught, "callback exception was swallowed");

        require(initMessagesBatcher(2).has_value(), "queue reservation failed");
        require(queueMessage(event2OutboundMessage(Event(8, 10), {}, Target::single(0))).has_value(), "queue failed");
        require(!flushAll(manager), "invalid send succeeded");
        require(flushAll(manager).has_value(), "failed flush retained messages");
        require(queueMessage(event2OutboundMessage(Event(8, 10), {}, target)).has_value(), "queue failed");
        clearMessagesBatcher();
        clearMessagesBatcher();
        require(flushAll(manager).has_value(), "clear retained messages");
#ifdef LIGHTNERX_SOURCE_TEST
        reportedPayloadSize = std::numeric_limits<std::size_t>::max();
        const auto oversized = event2OutboundMessage(Event(8, 10), {}, target);
        require(!oversized && oversized.error() == lightnerx::ErrorCode::MessageSize,
                "allocation size overflow was not rejected");
        reportedPayloadSize.reset();
        failAllocation = true;
        const std::array<std::function<lightnerx::Result<OutboundMessage>()>, 5> converters{
            [&] { return event2OutboundMessage(Event(8, 10), {}, target); },
            [&] { return intention2OutboundMessage(Intention(6, 10, 1), {}, target); },
            [&] { return notification2OutboundMessage(Notification(4, 10), {}, target); },
            [&] { return response2OutboundMessage(Response(11, 1, APPROVED_RESPONSE), {}, target); },
            [&] { return unsuccessfulResponse2OutboundMessage(UnsuccessfulResponse(15, 1, DENIED_RESPONSE, 1), {}, target); }
        };
        for (const auto& convert : converters) {
            const auto result = convert();
            require(!result && result.error() == lightnerx::ErrorCode::NotEnoughMemory,
                    "converter did not report allocation failure");
        }
        failAllocation = false;
        for (int completed = 0; completed < 2; ++completed) {
            require(queueMessage(event2OutboundMessage(Event(8, 10), {}, target)).has_value(), "queue failed");
            require(queueMessage(event2OutboundMessage(Event(8, 10), {}, target)).has_value(), "queue failed");
            throwSend = true;
            sendsBeforeThrow = completed;
            caught = false;
            try { (void)flushAll(manager); } catch (int value) { caught = value == 23; }
            require(caught, "send exception swallowed");
            require(liveMessages == 0, "exception leaked queued messages");
        }
        throwSend = false;
        require(queueMessage(event2OutboundMessage(Event(8, 10), {}, target)).has_value(), "queue failed");
        require(cleanupHook != nullptr, "network cleanup hook missing");
        cleanupHook();
        require(liveMessages == 0, "shutdown leaked queued messages");
#else
        manager.remove(*manager.getClientId(sender));
        SteamNetworkingSockets()->CloseConnection(sender, 0, nullptr, false);
        SteamNetworkingSockets()->CloseConnection(peer, 0, nullptr, false);
        networkKill();
#endif
        std::cout << "message parsing, callback, allocation and batching tests passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
