#ifndef LIGHTNERX_NETWORK_TRANSPORT_HPP
#define LIGHTNERX_NETWORK_TRANSPORT_HPP

#include <string_view>
#include <vector>

#include <lightnerx/Result.hpp>
#include <lightnerx/network/connection_handle.hpp>
#include <lightnerx/network/connection_manager.hpp>
#include <lightnerx/network/network_manager.hpp>

namespace lightnerx::network {
    Result<void> sendToConnection(
        std::string_view message,
        const ConnectionHandle hConn
    );

    Result<void> sendToConnections(
        std::string_view message,
        const std::vector<ConnectionHandle> connections
    );

    Result<void> broadcast(
        std::string_view message,
        const lightnerx::network::ConnectionManager& connectionManager,
        const std::vector<ConnectionHandle> exceptions
    );
}
#endif
