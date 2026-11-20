#ifndef LIGHTNERX_COMMUNICATION_MESSAGE_HPP
#define LIGHTNERX_COMMUNICATION_MESSAGE_HPP

#include <cstdint>
#include <deque>
#include <functional>
#include <string_view>
#include <variant>

#include <lightnerx/Result.hpp>
#include <lightnerx/network/message_batch.hpp>
#include <lightnerx/network/target.hpp>
#include <lightnerx/network/connection_manager.hpp>
#include <lightnerx/network/connection_handle.hpp>
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
        std::string_view payload;
    };

    class MessageInterpreter {
    private:
        using AuthorityInterpretedMessageCallback = std::function<void(Message)>;
        std::deque<AuthorityInterpretedMessageCallback> authorityInterpretedMessageCallbacks;

        using ObedientInterpretedMessageCallback = std::function<void(Message, std::uint16_t)>;
        std::deque<ObedientInterpretedMessageCallback> obedientInterpretedMessageCallbacks;

    public:
        void addAuthorityInterpretedMessageCallback(AuthorityInterpretedMessageCallback callback);

        void addObedientInterpretedMessageCallback(ObedientInterpretedMessageCallback callback);

        void obedientInterpret(
            std::string_view data,
            const lightnerx::communication::NotificationRegistry& registry,
            network::ConnectionHandle hConn,
            const lightnerx::network::ConnectionManager& connManager
        );

        void authorityInterpret(std::string_view data);

    private:
        void callAuthorityInterpretedCallbacks(Message message);

        void callObedientInterpretedCallbacks(Message message, std::uint16_t id);
    };

    Result<network::OutboundMessage> event2OutboundMessage(
        const lightnerx::communication::Event& event,
        std::string_view payload,
        lightnerx::network::Target target
    );

    Result<network::OutboundMessage> intention2OutboundMessage(
        const lightnerx::communication::Intention& intention,
        std::string_view payload,
        lightnerx::network::Target target
    );

    Result<network::OutboundMessage> response2OutboundMessage(
        const lightnerx::communication::Response& response,
        std::string_view payload,
        lightnerx::network::Target target
    );

    Result<network::OutboundMessage> unsuccessfulResponse2OutboundMessage(
        const lightnerx::communication::UnsuccessfulResponse& response,
        std::string_view payload,
        lightnerx::network::Target target
    );

    Result<network::OutboundMessage> notification2OutboundMessage(
        const lightnerx::communication::Notification& notification,
        std::string_view payload,
        lightnerx::network::Target target
    );
}
#endif
