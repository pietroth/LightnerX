module;

#include "steam/isteamnetworkingsockets.h"
#include "steam/steamclientpublic.h"
#include "steam/steamnetworkingtypes.h"

export module network.transport;

import std;

import network.connection_manager;
import network.network_manager;

export namespace network {
    constexpr HSteamNetConnection k_invalidConnection = 0;

    std::expected<void, std::error_code> sendToConnection(
        const std::span<const std::byte> message,
        const HSteamNetConnection hConn
    ) {
        if (!network::isNetworkInitialized()) {
            return std::unexpected(std::make_error_code(std::errc::resource_unavailable_try_again));
        }

        ISteamNetworkingSockets* pInterface = SteamNetworkingSockets();

        if (!pInterface || hConn == k_invalidConnection) {
            return std::unexpected(
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

        return std::unexpected(
            std::make_error_code(std::errc::io_error)
        );
    }

    std::expected<void, std::error_code> sendToConnections(
        const std::span<const std::byte> message,
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
            return std::unexpected(*error);
        }

        return {};
    }

    std::expected<void, std::error_code> broadcast(
        const std::span<const std::byte> message,
        const network::ConnectionManager& connectionManager,
        const std::vector<HSteamNetConnection> exceptions
    ) {
        std::optional<std::error_code> error;

        connectionManager.forEach([&](uint16_t id, HSteamNetConnection hConn) {
            if (std::ranges::contains(exceptions, hConn)) {
                return ;
            }

            auto result = sendToConnection(message, hConn);

            if (!result && !error) {
                error = result.error();
            }
        });

        if (error) {
            return std::unexpected(*error);
        }

        return {};
    }
}
