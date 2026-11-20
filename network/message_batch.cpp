#include <lightnerx/network/message_batch.hpp>

#include <lightnerx/network/network_manager.hpp>
#include <lightnerx/network/transport.hpp>

#include <steam/isteamnetworkingutils.h>
#include <cstddef>
#include <cstring>
#include <limits>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace lightnerx::network {

    OutboundMessage::OutboundMessage(SteamNetworkingMessage_t* message, Target target)
        : message(message), target(std::move(target)) {}

    void OutboundMessage::ReleaseMessage::operator()(SteamNetworkingMessage_t* message) const noexcept {
        message->Release();
    }

    Result<OutboundMessage> OutboundMessage::create(
        std::string_view header, std::string_view payload, Target target
    ) {
        constexpr auto maxSize = static_cast<std::size_t>(std::numeric_limits<int>::max());
        if (header.size() > maxSize || payload.size() > maxSize - header.size()) {
            return Result<OutboundMessage>::failure(ErrorCode::MessageSize);
        }

        auto* utils = SteamNetworkingUtils();
        if (!utils) return Result<OutboundMessage>::failure(ErrorCode::IoError);
        auto* message = utils->AllocateMessage(static_cast<int>(header.size() + payload.size()));
        if (!message) return Result<OutboundMessage>::failure(ErrorCode::NotEnoughMemory);

        auto* buffer = static_cast<char*>(message->m_pData);
        if (!header.empty()) std::memcpy(buffer, header.data(), header.size());
        if (!payload.empty()) std::memcpy(buffer + header.size(), payload.data(), payload.size());
        return OutboundMessage(message, std::move(target));
    }

    std::string_view OutboundMessage::data() const noexcept {
        if (!message || message->m_cbSize == 0) return {};
        return {static_cast<const char*>(message->m_pData), static_cast<std::size_t>(message->m_cbSize)};
    }

    struct MessageQueue {
        std::vector<OutboundMessage> messages;

        ~MessageQueue() {
            detail::setNetworkCleanup(nullptr);
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
        pendingQueue().messages.clear();
    }

    Result<void> initMessagesBatcher(int capacity) {
        if (capacity < 0) return Result<void>::failure(ErrorCode::InvalidArgument);
        auto& messages = pendingQueue().messages;
        if (static_cast<std::size_t>(capacity) > messages.max_size()) {
            return Result<void>::failure(ErrorCode::NotEnoughMemory);
        }
        messages.reserve(static_cast<std::size_t>(capacity));
        detail::setNetworkCleanup(&clearMessagesBatcher);
        return {};
    }

    Result<void> queueMessage(
        OutboundMessage message
    ) {
        detail::setNetworkCleanup(&clearMessagesBatcher);
        if (message.message == nullptr) {
            return Result<void>::failure(ErrorCode::InvalidArgument);
        }

        auto& messages = pendingQueue().messages;
        if (messages.size() >= messages.capacity()) {
            return Result<void>::failure(ErrorCode::NotEnoughMemory);
        }

        messages.push_back(std::move(message));
        return {};
    }

    Result<void> queueMessage(Result<OutboundMessage> message) {
        if (!message) return Result<void>::failure(message.error());
        return queueMessage(std::move(message).value());
    }

    Result<void> flushAll(
        const ConnectionManager& connectionManager
    ) {
        struct ReleasePending {
            ~ReleasePending() { clearMessagesBatcher(); }
        } releasePending;
        Result<void> lastError;

        for (auto& message : pendingQueue().messages) {
            const auto data = message.data();

            Result<void> result;

            if (message.target.isSingle()) {
                result = lightnerx::network::sendToConnection(
                    data,
                    std::get<ConnectionHandle>(message.target.connection)
                );
            } else if (message.target.isGroup()) {
                result = lightnerx::network::sendToConnections(
                    data,
                    std::get<std::vector<ConnectionHandle>>(message.target.connection)
                );
            } else {
                result = lightnerx::network::broadcast(
                    data,
                    connectionManager,
                    message.target.broadcastExceptions
                );
            }

            if (!result) {
                lastError = result;
            }
            message.message.reset();
        }

        return lastError;
    }
}
