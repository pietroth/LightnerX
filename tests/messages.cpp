#ifndef LIGHTNERX_SOURCE_TEST
#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
import std;
import lightnerx.communication.message;
import lightnerx.communication.event;
import lightnerx.communication.notification;
import lightnerx.communication.intention;
import lightnerx.communication.response;
import lightnerx.network.target;
import lightnerx.network.message_batch;
import lightnerx.network.connection_manager;
import lightnerx.network.network_manager;
#endif

using namespace lightnerx::communication;
using namespace lightnerx::network;

void require(bool condition, const char* description) {
    if (!condition) throw std::runtime_error(description);
}

std::vector<std::byte> bytes(OutboundMessage outbound) {
    auto* message = outbound.message;
    std::vector<std::byte> result(static_cast<std::byte*>(message->m_pData),
                                  static_cast<std::byte*>(message->m_pData) + message->m_cbSize);
    message->Release();
    return result;
}

void setSize(std::vector<std::byte>& buffer, std::uint16_t size) {
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
        const std::array payload{std::byte{42}, std::byte{7}};
        const auto target = Target::single(sender);
        require(target.isSingle() && !target.isGroup() && !target.isBroadcast(), "single target");
        require(Target::group({sender}).isGroup(), "group target");
        require(Target::broadcast().isBroadcast(), "broadcast target");
        auto event = bytes(event2OutboundMessage(Event(12, 10), payload, target));
        auto response = bytes(response2OutboundMessage(Response(11, 20, APPROVED_RESPONSE), {}, target));
        auto failure = bytes(response2OutboundMessage(Response(11, 20, FAILURE_RESPONSE), {}, target));
        auto denied = bytes(unsuccessfulResponse2OutboundMessage(UnsuccessfulResponse(15, 20, DENIED_RESPONSE, 9), {}, target));
        auto unsuccessful = bytes(unsuccessfulResponse2OutboundMessage(UnsuccessfulResponse(19, 20, PARTIALLY_APPROVED_RESPONSE, 9), payload, target));
        auto notification = bytes(notification2OutboundMessage(Notification(6, 47), payload, target));
        auto intention = bytes(intention2OutboundMessage(Intention(8, 48, 20), payload, target));
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
                interpreter.authorityInterpret(std::span{valid}.first(size));
            auto bad = valid;
            setSize(bad, std::to_integer<unsigned>(valid[0]) & 1u);
            interpreter.authorityInterpret(bad);
            setSize(bad, static_cast<std::uint16_t>(65534 | (std::to_integer<unsigned>(valid[0]) & 1u)));
            interpreter.authorityInterpret(bad);
            bad = valid;
            bad.push_back(std::byte{});
            interpreter.authorityInterpret(bad);
            require(authorityCount == before + 1, "malformed authority message accepted");
        }
        for (const auto& valid : {notification, intention}) {
            const auto before = obedientCount;
            interpreter.obedientInterpret(valid, registry, sender, manager);
            require(obedientCount == before + 1, "valid obedient message rejected");
            for (std::size_t size = 0; size < valid.size(); ++size)
                interpreter.obedientInterpret(std::span{valid}.first(size), registry, sender, manager);
            auto bad = valid;
            setSize(bad, 0);
            interpreter.obedientInterpret(bad, registry, sender, manager);
            setSize(bad, 65535);
            interpreter.obedientInterpret(bad, registry, sender, manager);
            bad = valid;
            bad.insert(bad.end(), valid.begin(), valid.end());
            interpreter.obedientInterpret(bad, registry, sender, manager);
            interpreter.obedientInterpret(valid, registry, 0, manager);
            require(obedientCount == before + 1, "malformed or unidentified obedient message accepted");
        }
        auto unknownStatus = response;
        unknownStatus[4] = std::byte{255};
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
        const std::vector<std::byte> authorityPayload(32767 - sizeof(Event));
        auto largestEvent = bytes(event2OutboundMessage(Event(65534, 10), authorityPayload, target));
        const auto beforeBoundaryAuthority = authorityCount;
        interpreter.authorityInterpret(largestEvent);
        largestEvent.push_back(std::byte{});
        interpreter.authorityInterpret(largestEvent);
        require(authorityCount == beforeBoundaryAuthority + 1, "authority length boundary");
        const std::vector<std::byte> obedientPayload(65535 - sizeof(Notification));
        auto largestNotification = bytes(notification2OutboundMessage(Notification(65535, 47), obedientPayload, target));
        const auto beforeBoundaryObedient = obedientCount;
        interpreter.obedientInterpret(largestNotification, registry, sender, manager);
        largestNotification.push_back(std::byte{});
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

        initMessagesBatcher(2);
        require(queueMessage(event2OutboundMessage(Event(8, 10), {}, Target::single(0))).has_value(), "queue failed");
        require(!flushAll(manager), "invalid send succeeded");
        require(flushAll(manager).has_value(), "failed flush retained messages");
        require(queueMessage(event2OutboundMessage(Event(8, 10), {}, target)).has_value(), "queue failed");
        clearMessagesBatcher();
        clearMessagesBatcher();
        require(flushAll(manager).has_value(), "clear retained messages");
#ifdef LIGHTNERX_SOURCE_TEST
        caught = false;
        try {
            lightnerx::communication::detail::allocateMessage(sizeof(Event), std::numeric_limits<std::size_t>::max());
        } catch (const std::length_error&) { caught = true; }
        require(caught, "allocation size overflow was not rejected");
        failAllocation = true;
        int badAllocCount = 0;
        const std::array<std::function<OutboundMessage()>, 5> converters{
            [&] { return event2OutboundMessage(Event(8, 10), {}, target); },
            [&] { return intention2OutboundMessage(Intention(6, 10, 1), {}, target); },
            [&] { return notification2OutboundMessage(Notification(4, 10), {}, target); },
            [&] { return response2OutboundMessage(Response(11, 1, APPROVED_RESPONSE), {}, target); },
            [&] { return unsuccessfulResponse2OutboundMessage(UnsuccessfulResponse(15, 1, DENIED_RESPONSE, 1), {}, target); }
        };
        for (const auto& convert : converters) {
            try { convert(); } catch (const std::bad_alloc&) { ++badAllocCount; }
        }
        require(badAllocCount == 5, "not every converter reports allocation failure");
        failAllocation = false;
        for (int completed = 0; completed < 2; ++completed) {
            require(queueMessage(event2OutboundMessage(Event(8, 10), {}, target)).has_value(), "queue failed");
            require(queueMessage(event2OutboundMessage(Event(8, 10), {}, target)).has_value(), "queue failed");
            throwSend = true;
            sendsBeforeThrow = completed;
            caught = false;
            try { flushAll(manager); } catch (int value) { caught = value == 23; }
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
