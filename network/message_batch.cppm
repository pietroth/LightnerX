module;

#include "steam/isteamnetworkingutils.h"
#include <steam/steamnetworkingtypes.h>

export module lightnerx.network.message_batch;

import std;

import lightnerx.network.transport;
import lightnerx.network.target;
import lightnerx.network.connection_manager;

namespace lightnerx::network {
    export struct OutboundMessage {
        SteamNetworkingMessage_t* message;
        Target target;
    };

    std::vector<OutboundMessage> messages;

    export void initMessagesBatcher(int capacity) {
        messages.reserve(capacity);
    }

    export std::expected<void, std::error_code> queueMessage(
        OutboundMessage message
    ) {
        if (message.message == nullptr) {
            return std::unexpected(std::make_error_code(std::errc::invalid_argument));
        }

        if (messages.size() >= messages.capacity()) {
            message.message->Release();
            return std::unexpected(std::make_error_code(std::errc::not_enough_memory));
        }

        messages.push_back(std::move(message));
        return {};
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
                result = lightnerx::network::sendToConnection(
                    data,
                    std::get<HSteamNetConnection>(message.target.connection)
                );
            } else if (std::holds_alternative<std::vector<HSteamNetConnection>>(message.target.connection)) {
                result = lightnerx::network::sendToConnections(
                    data,
                    std::get<std::vector<HSteamNetConnection>>(message.target.connection)
                );
            } else {
                result = lightnerx::network::broadcast(
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
