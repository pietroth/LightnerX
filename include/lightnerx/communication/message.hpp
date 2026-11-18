#ifndef LIGHTNERX_COMMUNICATION_MESSAGE_HPP
#define LIGHTNERX_COMMUNICATION_MESSAGE_HPP

#include "steam/isteamnetworkingutils.h"
#include "steam/steamnetworkingtypes.h"
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <tcb/span.hpp>
#include <variant>

#include <lightnerx/network/message_batch.hpp>
#include <lightnerx/network/target.hpp>
#include <lightnerx/network/connection_manager.hpp>
#include <lightnerx/communication/notification.hpp>
#include <lightnerx/communication/intention.hpp>
#include <lightnerx/communication/response.hpp>
#include <lightnerx/communication/event.hpp>

namespace lightnerx::communication {
    struct Message {
        std::variant<
            lightnerx::communication::Event,
            lightnerx::communication::Intention,
            lightnerx::communication::Response,
            lightnerx::communication::UnsuccessfulResponse,
            lightnerx::communication::Notification
        > header;
        tcb::span<const std::byte> payload;
    };

    class MessageInterpreter {
    private:
        using AuthorityInterpretedMessageCallback = std::function<void(Message)>;
        std::deque<AuthorityInterpretedMessageCallback> authorityInterpretedMessageCallbacks;

        using ObedientInterpretedMessageCallback = std::function<void(Message, uint16_t)>;
        std::deque<ObedientInterpretedMessageCallback> obedientInterpretedMessageCallbacks;

    public:
        void addAuthorityInterpretedMessageCallback(AuthorityInterpretedMessageCallback callback);

        void addObedientInterpretedMessageCallback(ObedientInterpretedMessageCallback callback);

        void obedientInterpret(
            const tcb::span<const std::byte> data,
            const lightnerx::communication::NotificationRegistry& registry,
            HSteamNetConnection hConn,
            const lightnerx::network::ConnectionManager& connManager
        );

        void authorityInterpret(const tcb::span<const std::byte> data);

    private:
        void callAuthorityInterpretedCallbacks(Message message);

        void callObedientInterpretedCallbacks(Message message, uint16_t id);
    };

    lightnerx::network::OutboundMessage event2OutboundMessage(
        const lightnerx::communication::Event& event,
        const tcb::span<const std::byte> payload,
        lightnerx::network::Target target
    );

    lightnerx::network::OutboundMessage intention2OutboundMessage(
        const lightnerx::communication::Intention& intention,
        const tcb::span<const std::byte> payload,
        lightnerx::network::Target target
    );

    lightnerx::network::OutboundMessage response2OutboundMessage(
        const lightnerx::communication::Response& response,
        const tcb::span<const std::byte> payload,
        lightnerx::network::Target target
    );

    lightnerx::network::OutboundMessage unsuccessfulResponse2OutboundMessage(
        const lightnerx::communication::UnsuccessfulResponse& response,
        const tcb::span<const std::byte> payload,
        lightnerx::network::Target target
    );

    lightnerx::network::OutboundMessage notification2OutboundMessage(
        const lightnerx::communication::Notification& notification,
        const tcb::span<const std::byte> payload,
        lightnerx::network::Target target
    );
}
#endif
