#include <lightnerx/network/transport.hpp>

#include <steam/steamnetworkingsockets.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <vector>

namespace lightnerx::network {

Result<void> sendToConnection(
        std::string_view message,
        const ConnectionHandle hConn
    ) {
    if (!lightnerx::network::isNetworkInitialized()) {
        return Result<void>::failure(ErrorCode::ResourceUnavailableTryAgain);
    }

    ISteamNetworkingSockets* pInterface = SteamNetworkingSockets();

    if (!pInterface || hConn == k_invalidConnection) {
        return Result<void>::failure(ErrorCode::NotConnected);
    }

    const auto messageSize = message.size();
    if (messageSize > std::numeric_limits<std::uint32_t>::max()) {
        return Result<void>::failure(ErrorCode::MessageSize);
    }

    EResult result = pInterface->SendMessageToConnection(
        hConn,
        message.data(),
        static_cast<std::uint32_t>(messageSize),
        k_nSteamNetworkingSend_Reliable,
        nullptr
    );

    if (result == k_EResultOK) {
        return {};
    }

    return Result<void>::failure(ErrorCode::IoError);
}

Result<void> sendToConnections(
        std::string_view message,
        const std::vector<ConnectionHandle> connections
    ) {
    std::optional<ErrorCode> error;

    for (auto conn : connections) {
        auto result = sendToConnection(message, conn);

        if (!result && !error) {
            error = result.error();
        }
    }

    if (error) {
        return Result<void>::failure(*error);
    }

    return {};
}

Result<void> broadcast(
        std::string_view message,
        const lightnerx::network::ConnectionManager& connectionManager,
        const std::vector<ConnectionHandle> exceptions
    ) {
    std::optional<ErrorCode> error;

    connectionManager.forEach([&](std::uint16_t, ConnectionHandle hConn) {
        if (std::find(exceptions.begin(), exceptions.end(), hConn) != exceptions.end()) {
            return ;
        }

        auto result = sendToConnection(message, hConn);

        if (!result && !error) {
            error = result.error();
        }
    });

    if (error) {
        return Result<void>::failure(*error);
    }

    return {};
}

}

