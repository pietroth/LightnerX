#include <lightnerx/network/message_batch.hpp>

#include <cstddef>
#include <zeus/expected.hpp>
#include <tcb/span.hpp>
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

    namespace {
        MessageQueue& pendingQueue() noexcept {
            // A consumer can reserve space before main(), from another translation unit.
            static MessageQueue queue;
            return queue;
        }
    }

    void clearMessagesBatcher() noexcept {
        pendingQueue().clear();
    }

    void initMessagesBatcher(int capacity) {
        if (capacity < 0) throw std::invalid_argument("Negative batch capacity");
        detail::setNetworkCleanup(&clearMessagesBatcher);
        pendingQueue().messages.reserve(static_cast<std::size_t>(capacity));
    }

    zeus::expected<void, std::error_code> queueMessage(
        OutboundMessage message
    ) {
        detail::setNetworkCleanup(&clearMessagesBatcher);
        if (message.message == nullptr) {
            return zeus::unexpected(std::make_error_code(std::errc::invalid_argument));
        }

        auto& messages = pendingQueue().messages;
        if (messages.size() >= messages.capacity()) {
            message.message->Release();
            return zeus::unexpected(std::make_error_code(std::errc::not_enough_memory));
        }

        messages.push_back(std::move(message));
        return {};
    }

    zeus::expected<void, std::error_code> flushAll(
        const ConnectionManager& connectionManager
    ) {
        struct ReleasePending {
            ~ReleasePending() { clearMessagesBatcher(); }
        } releasePending;
        std::error_code lastError;

        for (auto& message : pendingQueue().messages) {
            tcb::span<const std::byte> data{
                static_cast<const std::byte*>(message.message->m_pData),
                static_cast<size_t>(message.message->m_cbSize)
            };

            zeus::expected<void, std::error_code> result;

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
            return zeus::unexpected(lastError);
        }

        return {};
    }
}
