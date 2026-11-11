module;

#include "steam/isteamnetworkingutils.h"
#include "steam/steamnetworkingtypes.h"

export module lightnerx.communication.message;

import std;

import lightnerx.network.message_batch;
import lightnerx.network.target;
import lightnerx.network.connection_manager;
import lightnerx.communication.notification;
import lightnerx.communication.intention;
import lightnerx.communication.response;
import lightnerx.communication.event;

export namespace lightnerx::communication {
    struct Message {
        std::variant<
            lightnerx::communication::Event,
            lightnerx::communication::Intention,
            lightnerx::communication::Response,
            lightnerx::communication::UnsuccessfulResponse,
            lightnerx::communication::Notification
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
            const lightnerx::communication::NotificationRegistry& registry,
            HSteamNetConnection hConn,
            const lightnerx::network::ConnectionManager& connManager
        ) {
            if (data.size_bytes() < 4) {
                return;
            }

            uint16_t size;
            std::memcpy(&size, data.data(), sizeof(size));

            uint16_t type;
            std::memcpy(&type, data.data() + 2, sizeof(type));

            if (registry.exist(type)) {
                lightnerx::communication::Notification notification = Notification(size, type);
                Message message {notification, data.subspan(sizeof(lightnerx::communication::Notification))};

                std::optional<uint16_t> id_opt = connManager.getClientId(hConn);
                if (id_opt.has_value()) {
                    uint16_t id = id_opt.value();
                    callObedientInterpretedCallbacks(message, id);
                }

                return;
            } else {
                if (data.size_bytes() < sizeof(lightnerx::communication::Intention)) {
                    return;
                }

                uint16_t correlationId;
                std::memcpy(&correlationId, data.data() + 4, sizeof(correlationId));

                lightnerx::communication::Intention intention = Intention(size, type, correlationId);
                Message message {intention, data.subspan(sizeof(lightnerx::communication::Intention))};

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

                lightnerx::communication::Event event = Event(size, typeId);
                Message message {event, data.subspan(sizeof(lightnerx::communication::Event))};

                callAuthorityInterpretedCallbacks(message);
            } else {
                // response

                if (data.size_bytes() < sizeof(lightnerx::communication::Response)) {
                    return;
                }

                uint16_t correlationId;
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
                        if (data.size_bytes() < sizeof(lightnerx::communication::UnsuccessfulResponse)) {
                            return;
                        }

                        uint16_t reasonCode;
                        std::memcpy(&reasonCode, data.data() + 5, sizeof(reasonCode));

                        lightnerx::communication::UnsuccessfulResponse unsuccessfulResponse =
                            UnsuccessfulResponse(size, correlationId, status, reasonCode);
                        Message message {unsuccessfulResponse, data.subspan(sizeof(lightnerx::communication::UnsuccessfulResponse))};

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

    lightnerx::network::OutboundMessage event2OutboundMessage(
        const lightnerx::communication::Event& event,
        const std::span<const std::byte> payload,
        lightnerx::network::Target target
    ) {
        const auto size = sizeof(event) + payload.size();
        auto* message = SteamNetworkingUtils()->AllocateMessage(static_cast<int>(size));

        std::memcpy(message->m_pData, &event, sizeof(event));

        if (!payload.empty()) {
            std::memcpy(static_cast<std::byte*>(message->m_pData) + sizeof(event), payload.data(), payload.size());
        }

        return {message, std::move(target)};
    }

    lightnerx::network::OutboundMessage intention2OutboundMessage(
        const lightnerx::communication::Intention& intention,
        const std::span<const std::byte> payload,
        lightnerx::network::Target target
    ) {
        const auto size = sizeof(intention) + payload.size();
        auto* message = SteamNetworkingUtils()->AllocateMessage(static_cast<int>(size));

        std::memcpy(message->m_pData, &intention, sizeof(intention));

        if (!payload.empty()) {
            std::memcpy(static_cast<std::byte*>(message->m_pData) + sizeof(intention), payload.data(), payload.size());
        }

        return {message, std::move(target)};
    }

    lightnerx::network::OutboundMessage response2OutboundMessage(
        const lightnerx::communication::Response& response,
        const std::span<const std::byte> payload,
        lightnerx::network::Target target
    ) {
        const auto size = sizeof(response) + payload.size();
        auto* message = SteamNetworkingUtils()->AllocateMessage(static_cast<int>(size));

        std::memcpy(message->m_pData, &response, sizeof(response));

        if (!payload.empty()) {
            std::memcpy(static_cast<std::byte*>(message->m_pData) + sizeof(response), payload.data(), payload.size());
        }

        return {message, std::move(target)};
    }

    lightnerx::network::OutboundMessage unsuccessfulResponse2OutboundMessage(
        const lightnerx::communication::UnsuccessfulResponse& response,
        const std::span<const std::byte> payload,
        lightnerx::network::Target target
    ) {
        const auto size = sizeof(response) + payload.size();
        auto* message = SteamNetworkingUtils()->AllocateMessage(static_cast<int>(size));

        std::memcpy(message->m_pData, &response, sizeof(response));

        if (!payload.empty()) {
            std::memcpy(static_cast<std::byte*>(message->m_pData) + sizeof(response), payload.data(), payload.size());
        }

        return {message, std::move(target)};
    }

    lightnerx::network::OutboundMessage notification2OutboundMessage(
        const lightnerx::communication::Notification& notification,
        const std::span<const std::byte> payload,
        lightnerx::network::Target target
    ) {
        const auto size = sizeof(notification) + payload.size();
        auto* message = SteamNetworkingUtils()->AllocateMessage(static_cast<int>(size));

        std::memcpy(message->m_pData, &notification, sizeof(notification));

        if (!payload.empty()) {
            std::memcpy(
                static_cast<std::byte*>(message->m_pData) + sizeof(notification),
                payload.data(),
                payload.size()
            );
        }

        return {message, std::move(target)};
    }
}
