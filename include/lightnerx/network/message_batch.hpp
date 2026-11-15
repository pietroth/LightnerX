#ifndef LIGHTNERX_NETWORK_MESSAGE_BATCH_HPP
#define LIGHTNERX_NETWORK_MESSAGE_BATCH_HPP

#include "steam/isteamnetworkingutils.h"
#include <steam/steamnetworkingtypes.h>
#include <lightnerx/compact/expected.hpp>
#include <system_error>

#include <lightnerx/network/transport.hpp>
#include <lightnerx/network/network_manager.hpp>
#include <lightnerx/network/target.hpp>
#include <lightnerx/network/connection_manager.hpp>

namespace lightnerx::network {
    struct OutboundMessage {
        SteamNetworkingMessage_t* message;
        Target target;
    };
    void clearMessagesBatcher() noexcept;
    void initMessagesBatcher(int capacity);
    lightnerx::compact::expected<void, std::error_code> queueMessage(OutboundMessage message);
    lightnerx::compact::expected<void, std::error_code> flushAll(const ConnectionManager& connectionManager);
}

#endif
