#ifndef LIGHTNERX_NETWORK_MESSAGE_BATCH_HPP
#define LIGHTNERX_NETWORK_MESSAGE_BATCH_HPP

#include <memory>
#include <string_view>

#include <lightnerx/Result.hpp>
#include <lightnerx/network/target.hpp>

struct SteamNetworkingMessage_t;

namespace lightnerx::network {
    class ConnectionManager;

    class OutboundMessage {
    public:
        static Result<OutboundMessage> create(
            std::string_view header, std::string_view payload, Target target
        );

        // The view is valid while this object retains the message buffer.
        [[nodiscard]] std::string_view data() const noexcept;

    private:
        struct ReleaseMessage {
            void operator()(SteamNetworkingMessage_t* message) const noexcept;
        };

        OutboundMessage(SteamNetworkingMessage_t* message, Target target);

        std::unique_ptr<SteamNetworkingMessage_t, ReleaseMessage> message;
        Target target;

        friend Result<void> queueMessage(OutboundMessage message);
        friend Result<void> flushAll(const ConnectionManager& connectionManager);
    };

    void clearMessagesBatcher() noexcept;
    Result<void> initMessagesBatcher(int capacity);
    Result<void> queueMessage(OutboundMessage message);
    Result<void> queueMessage(Result<OutboundMessage> message);
    Result<void> flushAll(const ConnectionManager& connectionManager);
}

#endif
