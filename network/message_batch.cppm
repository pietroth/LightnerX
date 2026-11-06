module;

#include "steam/isteamnetworkingutils.h"
#include <steam/steamnetworkingtypes.h>

export module network.message_batch;

import std;

import network.transport;
import network.target;
import network.connection_manager;

namespace network {
    export struct OutboundMessage {
        SteamNetworkingMessage_t* message;
        Target target;
    };

    std::vector<OutboundMessage> messages;

    export void initMessagesBatcher(int capacity) {
        messages.reserve(capacity);
    }

    export std::expected<void, std::error_code> queueMessage(
        const std::span<const std::byte> message,
        Target target
    ) {
        if (messages.size() >= messages.capacity()) {
            return std::unexpected(std::make_error_code(std::errc::not_enough_memory));
        }

        SteamNetworkingMessage_t* networkingMessage = SteamNetworkingUtils()->AllocateMessage(
            static_cast<int>(message.size())
        );

        if (networkingMessage != nullptr) {
            std::memcpy(networkingMessage->m_pData, message.data(), message.size());

            messages.push_back(OutboundMessage{
                networkingMessage,
                std::move(target)
            });

            return {};
        }

        return std::unexpected(std::make_error_code(std::errc::not_enough_memory));
    }

    export std::expected<void, std::error_code> flushAll(
        const ConnectionManager& connectionManager
    ) {
        std::error_code lastError;

        for (const auto& message : messages) {
            std::span<const std::byte> data{
                static_cast<const std::byte*>(message.message->m_pData),
                static_cast<size_t>(message.message->m_cbSize)
            };

            std::expected<void, std::error_code> result;

            if (message.target.isSingle()) {
                result = network::sendToConnection(
                    data,
                    std::get<HSteamNetConnection>(message.target.connection)
                );
            } else if (std::holds_alternative<std::vector<HSteamNetConnection>>(message.target.connection)) {
                result = network::sendToConnections(
                    data,
                    std::get<std::vector<HSteamNetConnection>>(message.target.connection)
                );
            } else {
                result = network::broadcast(
                    data,
                    connectionManager,
                    message.target.broadcastExceptions
                );
            }

            if (!result) {
                lastError = result.error();
            }

            if (message.message != nullptr) {
                message.message->Release();
            }
        }

        messages.clear();

        if (lastError) {
            return std::unexpected(lastError);
        }

        return {};
    }
}
