#include <lightnerx/network/transport.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <lightnerx/compact/expected.hpp>
#include <optional>
#include <tcb/span.hpp>
#include <system_error>
#include <vector>

namespace lightnerx::network {

lightnerx::compact::expected<void, std::error_code> sendToConnection(
        const tcb::span<const std::byte> message,
        const HSteamNetConnection hConn
    ) {
    if (!lightnerx::network::isNetworkInitialized()) {
        return lightnerx::compact::unexpected(std::make_error_code(std::errc::resource_unavailable_try_again));
    }

    ISteamNetworkingSockets* pInterface = SteamNetworkingSockets();

    if (!pInterface || hConn == k_invalidConnection) {
        return lightnerx::compact::unexpected(
            std::make_error_code(std::errc::not_connected)
        );
    }

    EResult result = pInterface->SendMessageToConnection(
        hConn,
        message.data(),
        static_cast<uint32_t>(message.size()),
        k_nSteamNetworkingSend_Reliable,
        nullptr
    );

    if (result == k_EResultOK) {
        return {};
    }

    return lightnerx::compact::unexpected(
        std::make_error_code(std::errc::io_error)
    );
}

lightnerx::compact::expected<void, std::error_code> sendToConnections(
        const tcb::span<const std::byte> message,
        const std::vector<HSteamNetConnection> connections
    ) {
    std::optional<std::error_code> error;

    for (auto conn : connections) {
        auto result = sendToConnection(message, conn);

        if (!result && !error) {
            error = result.error();
        }
    }

    if (error) {
        return lightnerx::compact::unexpected(*error);
    }

    return {};
}

lightnerx::compact::expected<void, std::error_code> broadcast(
        const tcb::span<const std::byte> message,
        const lightnerx::network::ConnectionManager& connectionManager,
        const std::vector<HSteamNetConnection> exceptions
    ) {
    std::optional<std::error_code> error;

    connectionManager.forEach([&](uint16_t id, HSteamNetConnection hConn) {
        if (std::find(exceptions.begin(), exceptions.end(), hConn) != exceptions.end()) {
            return ;
        }

        auto result = sendToConnection(message, hConn);

        if (!result && !error) {
            error = result.error();
        }
    });

    if (error) {
        return lightnerx::compact::unexpected(*error);
    }

    return {};
}

}

