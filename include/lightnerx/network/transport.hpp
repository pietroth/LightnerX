#ifndef LIGHTNERX_NETWORK_TRANSPORT_HPP
#define LIGHTNERX_NETWORK_TRANSPORT_HPP

#include "steam/isteamnetworkingsockets.h"
#include "steam/steamclientpublic.h"
#include "steam/steamnetworkingtypes.h"
#include <cstddef>
#include <zeus/expected.hpp>
#include <tcb/span.hpp>
#include <system_error>
#include <vector>

#include <lightnerx/network/connection_manager.hpp>
#include <lightnerx/network/network_manager.hpp>

namespace lightnerx::network {
    zeus::expected<void, std::error_code> sendToConnection(
        const tcb::span<const std::byte> message,
        const HSteamNetConnection hConn
    );

    zeus::expected<void, std::error_code> sendToConnections(
        const tcb::span<const std::byte> message,
        const std::vector<HSteamNetConnection> connections
    );

    zeus::expected<void, std::error_code> broadcast(
        const tcb::span<const std::byte> message,
        const lightnerx::network::ConnectionManager& connectionManager,
        const std::vector<HSteamNetConnection> exceptions
    );
}
#endif
