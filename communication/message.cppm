module;

#include "steam/isteamnetworkingutils.h"
#include "steam/steamnetworkingtypes.h"

#include <functional>
#include <vector>
#include <variant>
#include <span>
#include <cstdint>
#include <cstring>
#include <optional>

export module communication.message;

import network.message_batch;
import network.connection_manager;
import communication.notification;
import communication.intention;
import communication.response;
import communication.event;

export namespace communication {
    struct Message {
        std::variant<
            communication::Event,
            communication::Intention,
            communication::Response,
            communication::UnsuccessfulResponse,
            communication::Notification
        > header;
        std::span<const std::byte> payload;
    };

    class MessageInterpreter {
    private:
        using AuthorityInterpretedMessageCallback = std::function<void(Message)>;
        std::vector<AuthorityInterpretedMessageCallback> authorityInterpretedMessageCallbacks;

        using ObedientInterpretedMessageCallback = std::function<void(Message, uint16_t)>;
        std::vector<ObedientInterpretedMessageCallback> obedientInterpretedMessageCallbacks;

    public:
        void addAuthorityInterpretedMessageCallback(AuthorityInterpretedMessageCallback callback) {
            authorityInterpretedMessageCallbacks.push_back(callback);
        }

        void addObedientInterpretedMessageCallback(ObedientInterpretedMessageCallback callback) {
            obedientInterpretedMessageCallbacks.push_back(callback);
        }

        void obedientInterpret(
            const std::span<const std::byte> data,
            const communication::NotificationRegistry& registry,
            HSteamNetConnection hConn,
            const network::ConnectionManager& connManager
        ) {
            if (data.size_bytes() < 4) {
                return;
            }

            uint16_t size;
            std::memcpy(&size, data.data(), sizeof(size));

            uint16_t type;
            std::memcpy(&type, data.data() + 2, sizeof(type));

            if (registry.exist(type)) {
                communication::Notification notification = Notification(size, type);
                Message message {notification, data.subspan(sizeof(communication::Notification))};

                std::optional<uint16_t> id_opt = connManager.getClientId(hConn);
                if (id_opt.has_value()) {
                    uint16_t id = id_opt.value();
                    callObedientInterpretedCallbacks(message, id);
                }

                return;
            } else {
                if (data.size_bytes() < sizeof(communication::Intention)) {
                    return;
                }

                uint16_t correlationId;
                std::memcpy(&correlationId, data.data() + 4, sizeof(correlationId));

                communication::Intention intention = Intention(size, type, correlationId);
                Message message {intention, data.subspan(sizeof(communication::Intention))};

                std::optional<uint16_t> id_opt = connManager.getClientId(hConn);
                if (id_opt.has_value()) {
                    uint16_t id = id_opt.value();
                    callObedientInterpretedCallbacks(message, id);
                }

                return;
            }
        }

        void authorityInterpret(const std::span<const std::byte> data) {
            if (data.size_bytes() < 4) {
                return;
            }

            uint16_t base;
            std::memcpy(&base, data.data(), sizeof(base));

            int messageTypeFlag = base & 1;
            uint16_t size = base >> 1;

            if (!messageTypeFlag) {
                // event

                uint16_t typeId;
                std::memcpy(&typeId, data.data() + 2, sizeof(typeId));

                communication::Event event = Event(size, typeId);
                Message message {event, data.subspan(sizeof(communication::Event))};

                callAuthorityInterpretedCallbacks(message);
            } else {
                // response

                if (data.size_bytes() < sizeof(communication::Response)) {
                    return;
                }

                uint16_t correlationId;
                std::memcpy(&correlationId, data.data() + 2, sizeof(correlationId));

                uint8_t status;
                std::memcpy(&status, data.data() + 4, sizeof(status));

                switch (status) {
                    case communication::APPROVED_RESPONSE:
                    case communication::FAILURE_RESPONSE: {
                        communication::Response response = Response(size, correlationId, status);
                        Message message {response, {}};

                        callAuthorityInterpretedCallbacks(message);
                        break;
                    }

                    case communication::DENIED_RESPONSE:
                    case communication::PARTIALLY_APPROVED_RESPONSE: {
                        if (data.size_bytes() < sizeof(communication::UnsuccessfulResponse)) {
                            return;
                        }

                        uint16_t reasonCode;
                        std::memcpy(&reasonCode, data.data() + 5, sizeof(reasonCode));

                        communication::UnsuccessfulResponse unsuccessfulResponse =
                            UnsuccessfulResponse(size, correlationId, status, reasonCode);
                        Message message {unsuccessfulResponse, data.subspan(sizeof(communication::UnsuccessfulResponse))};

                        callAuthorityInterpretedCallbacks(message);
                        break;
                    }
                }
            }

            return;
        }

    private:
        void callAuthorityInterpretedCallbacks(Message message) {
            for (const auto& callback: authorityInterpretedMessageCallbacks) {
                callback(message);
            }
        }

        void callObedientInterpretedCallbacks(Message message, uint16_t id) {
            for (const auto& callback: obedientInterpretedMessageCallbacks) {
                callback(message, id);
            }
        }
    };

    network::OutboundMessage event2OutboundMessage(
        const communication::Event& event,
        const std::span<const std::byte> payload,
        HSteamNetConnection hConn
    ) {
        const auto size = sizeof(event) + payload.size();
        auto* message = SteamNetworkingUtils()->AllocateMessage(static_cast<int>(size));

        std::memcpy(message->m_pData, &event, sizeof(event));

        if (!payload.empty()) {
            std::memcpy(static_cast<std::byte*>(message->m_pData) + sizeof(event), payload.data(), payload.size());
        }

        return {message, hConn};
    }

    network::OutboundMessage intention2OutboundMessage(
        const communication::Intention& intention,
        const std::span<const std::byte> payload,
        HSteamNetConnection hConn
    ) {
        const auto size = sizeof(intention) + payload.size();
        auto* message = SteamNetworkingUtils()->AllocateMessage(static_cast<int>(size));

        std::memcpy(message->m_pData, &intention, sizeof(intention));

        if (!payload.empty()) {
            std::memcpy(static_cast<std::byte*>(message->m_pData) + sizeof(intention), payload.data(), payload.size());
        }

        return {message, hConn};
    }

    network::OutboundMessage response2OutboundMessage(
        const communication::Response& response,
        const std::span<const std::byte> payload,
        HSteamNetConnection hConn
    ) {
        const auto size = sizeof(response) + payload.size();
        auto* message = SteamNetworkingUtils()->AllocateMessage(static_cast<int>(size));

        std::memcpy(message->m_pData, &response, sizeof(response));

        if (!payload.empty()) {
            std::memcpy(static_cast<std::byte*>(message->m_pData) + sizeof(response), payload.data(), payload.size());
        }

        return {message, hConn};
    }

    network::OutboundMessage unsuccessfulResponse2OutboundMessage(
        const communication::UnsuccessfulResponse& response,
        const std::span<const std::byte> payload,
        HSteamNetConnection hConn
    ) {
        const auto size = sizeof(response) + payload.size();
        auto* message = SteamNetworkingUtils()->AllocateMessage(static_cast<int>(size));

        std::memcpy(message->m_pData, &response, sizeof(response));

        if (!payload.empty()) {
            std::memcpy(static_cast<std::byte*>(message->m_pData) + sizeof(response), payload.data(), payload.size());
        }

        return {message, hConn};
    }
}
