#include <lightnerx/network/message_batch.hpp>

#include <cstddef>
#include <expected>
#include <span>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

namespace lightnerx::network {

    struct MessageQueue {
        std::vector<OutboundMessage> messages;

        void clear() noexcept {
            for (const auto& message : messages) {
                if (message.message) message.message->Release();
            }
            messages.clear();
        }

        ~MessageQueue() {
            detail::setNetworkCleanup(nullptr);
            clear();
        }
    };

    MessageQueue pending;

    void clearMessagesBatcher() noexcept {
        pending.clear();
    }

    void initMessagesBatcher(int capacity) {
        if (capacity < 0) throw std::invalid_argument("Negative batch capacity");
        detail::setNetworkCleanup(&clearMessagesBatcher);
        pending.messages.reserve(static_cast<std::size_t>(capacity));
    }

    std::expected<void, std::error_code> queueMessage(
        OutboundMessage message
    ) {
        detail::setNetworkCleanup(&clearMessagesBatcher);
        if (message.message == nullptr) {
            return std::unexpected(std::make_error_code(std::errc::invalid_argument));
        }

        if (pending.messages.size() >= pending.messages.capacity()) {
            message.message->Release();
            return std::unexpected(std::make_error_code(std::errc::not_enough_memory));
        }

        pending.messages.push_back(std::move(message));
        return {};
    }

    std::expected<void, std::error_code> flushAll(
        const ConnectionManager& connectionManager
    ) {
        struct ReleasePending {
            ~ReleasePending() { clearMessagesBatcher(); }
        } releasePending;
        std::error_code lastError;

        for (auto& message : pending.messages) {
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
            } else if (message.target.isGroup()) {
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
            message.message->Release();
            message.message = nullptr;
        }

        if (lastError) {
            return std::unexpected(lastError);
        }

        return {};
    }
}
