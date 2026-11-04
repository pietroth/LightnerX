module;

#include "steam/isteamnetworkingutils.h"
#include <steam/steamnetworkingtypes.h>

#include <cstring>
#include <expected>
#include <system_error>
#include <vector>
#include <span>

export module network.message_batch;

import network.transport;

namespace network {
    export struct OutboundMessage {
        SteamNetworkingMessage_t* message;
        HSteamNetConnection connection;
    };

    std::vector<OutboundMessage> messages;

    export void initMessagesBatcher(int capacity) {
        messages.reserve(capacity);
    }

    export std::expected<void, std::error_code> queueMessage(
        const std::span<const std::byte> message,
        HSteamNetConnection hConn
    ) {
        if (messages.size() >= messages.capacity()) {
            return std::unexpected(std::make_error_code(std::errc::not_enough_memory));
        }

        OutboundMessage outboundMessage;

        SteamNetworkingMessage_t* networkingMessage = SteamNetworkingUtils()->AllocateMessage(
            static_cast<int>(message.size())
        );

        if (networkingMessage != nullptr) {
            std::memcpy(networkingMessage->m_pData, message.data(), message.size());

            outboundMessage.message = networkingMessage;
            outboundMessage.connection = hConn;

            messages.push_back(outboundMessage);

            return {};
        }

        return std::unexpected(std::make_error_code(std::errc::not_enough_memory));
    }

    export std::expected<void, std::error_code> flushAll() {
        std::error_code lastError;

        for (const auto& message : messages) {
            auto result = network::sendToConnection(
                std::span<const std::byte>{
                    static_cast<const std::byte*>(message.message->m_pData),
                    static_cast<size_t>(message.message->m_cbSize)
                },
                message.connection
            );

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
