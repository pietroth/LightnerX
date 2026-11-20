#include <lightnerx/communication/message.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string_view>
#include <utility>

namespace lightnerx::communication {

void MessageInterpreter::addAuthorityInterpretedMessageCallback(AuthorityInterpretedMessageCallback callback) {
    authorityInterpretedMessageCallbacks.push_back(callback);
}

void MessageInterpreter::addObedientInterpretedMessageCallback(ObedientInterpretedMessageCallback callback) {
    obedientInterpretedMessageCallbacks.push_back(callback);
}

void MessageInterpreter::obedientInterpret(
            std::string_view data,
            const lightnerx::communication::NotificationRegistry& registry,
            network::ConnectionHandle hConn,
            const lightnerx::network::ConnectionManager& connManager
        ) {
    if (data.size() < 4) {
        return;
    }

    std::uint16_t size;
    std::memcpy(&size, data.data(), sizeof(size));
    if (size != data.size()) {
        return;
    }

    std::uint16_t type;
    std::memcpy(&type, data.data() + 2, sizeof(type));

    if (registry.exist(type)) {
        lightnerx::communication::Notification notification = Notification(size, type);
        Message message {notification, data.substr(sizeof(lightnerx::communication::Notification))};

        std::optional<std::uint16_t> id_opt = connManager.getClientId(hConn);
        if (id_opt.has_value()) {
            std::uint16_t id = id_opt.value();
            callObedientInterpretedCallbacks(message, id);
        }

        return;
    } else {
        if (data.size() < sizeof(lightnerx::communication::Intention)) {
            return;
        }

        std::uint16_t correlationId;
        std::memcpy(&correlationId, data.data() + 4, sizeof(correlationId));

        lightnerx::communication::Intention intention = Intention(size, type, correlationId);
        Message message {intention, data.substr(sizeof(lightnerx::communication::Intention))};

        std::optional<std::uint16_t> id_opt = connManager.getClientId(hConn);
        if (id_opt.has_value()) {
            std::uint16_t id = id_opt.value();
            callObedientInterpretedCallbacks(message, id);
        }

        return;
    }
}

void MessageInterpreter::authorityInterpret(std::string_view data) {
    if (data.size() < 4) {
        return;
    }

    std::uint16_t base;
    std::memcpy(&base, data.data(), sizeof(base));

    int messageTypeFlag = base & 1;
    std::uint16_t size = base >> 1;
    if (size != data.size()) {
        return;
    }

    if (!messageTypeFlag) {
        // event

        std::uint16_t typeId;
        std::memcpy(&typeId, data.data() + 2, sizeof(typeId));

        lightnerx::communication::Event event = Event(size, typeId);
        Message message {event, data.substr(sizeof(lightnerx::communication::Event))};

        callAuthorityInterpretedCallbacks(message);
    } else {
        // response

        if (data.size() < sizeof(lightnerx::communication::Response)) {
            return;
        }

        std::uint16_t correlationId;
        std::memcpy(&correlationId, data.data() + 2, sizeof(correlationId));

        uint8_t status;
        std::memcpy(&status, data.data() + 4, sizeof(status));

        switch (status) {
            case lightnerx::communication::APPROVED_RESPONSE:
            case lightnerx::communication::FAILURE_RESPONSE: {
                lightnerx::communication::Response response = Response(size, correlationId, status);
                Message message {response, {}};

                callAuthorityInterpretedCallbacks(message);
                break;
            }

            case lightnerx::communication::DENIED_RESPONSE:
            case lightnerx::communication::PARTIALLY_APPROVED_RESPONSE: {
                if (data.size() < sizeof(lightnerx::communication::UnsuccessfulResponse)) {
                    return;
                }

                std::uint16_t reasonCode;
                std::memcpy(&reasonCode, data.data() + 5, sizeof(reasonCode));

                lightnerx::communication::UnsuccessfulResponse unsuccessfulResponse =
                    UnsuccessfulResponse(size, correlationId, status, reasonCode);
                Message message {unsuccessfulResponse, data.substr(sizeof(lightnerx::communication::UnsuccessfulResponse))};

                callAuthorityInterpretedCallbacks(message);
                break;
            }
        }
    }

    return;
}

void MessageInterpreter::callAuthorityInterpretedCallbacks(Message message) {
    const auto count = authorityInterpretedMessageCallbacks.size();
    for (std::size_t i = 0; i < count; ++i) {
        authorityInterpretedMessageCallbacks[i](message);
    }
}

void MessageInterpreter::callObedientInterpretedCallbacks(Message message, std::uint16_t id) {
    const auto count = obedientInterpretedMessageCallbacks.size();
    for (std::size_t i = 0; i < count; ++i) {
        obedientInterpretedMessageCallbacks[i](message, id);
    }
}

Result<network::OutboundMessage> event2OutboundMessage(
    const Event& event, std::string_view payload, network::Target target
) {
    return network::OutboundMessage::create(
        {reinterpret_cast<const char*>(&event), sizeof(event)}, payload, std::move(target)
    );
}

Result<network::OutboundMessage> intention2OutboundMessage(
    const Intention& intention, std::string_view payload, network::Target target
) {
    return network::OutboundMessage::create(
        {reinterpret_cast<const char*>(&intention), sizeof(intention)}, payload, std::move(target)
    );
}

Result<network::OutboundMessage> response2OutboundMessage(
    const Response& response, std::string_view payload, network::Target target
) {
    return network::OutboundMessage::create(
        {reinterpret_cast<const char*>(&response), sizeof(response)}, payload, std::move(target)
    );
}

Result<network::OutboundMessage> unsuccessfulResponse2OutboundMessage(
    const UnsuccessfulResponse& response, std::string_view payload, network::Target target
) {
    return network::OutboundMessage::create(
        {reinterpret_cast<const char*>(&response), sizeof(response)}, payload, std::move(target)
    );
}

Result<network::OutboundMessage> notification2OutboundMessage(
    const Notification& notification, std::string_view payload, network::Target target
) {
    return network::OutboundMessage::create(
        {reinterpret_cast<const char*>(&notification), sizeof(notification)}, payload, std::move(target)
    );
}

}

