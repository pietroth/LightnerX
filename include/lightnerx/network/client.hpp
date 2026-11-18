#ifndef LIGHTNERX_NETWORK_CLIENT_HPP
#define LIGHTNERX_NETWORK_CLIENT_HPP

#include "steam/isteamnetworkingsockets.h"
#include "steam/steamnetworkingtypes.h"
#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#include <cstddef>
#include <deque>
#include <zeus/expected.hpp>
#include <functional>
#include <tcb/span.hpp>
#include <string_view>
#include <system_error>

#include <lightnerx/network/network_manager.hpp>

namespace lightnerx::network {
    class Client {
    private:
        bool connected = false;
        bool updating = false;

        using ServerReceivedMessageCallback = std::function<void(tcb::span<const std::byte>)>;
        std::deque<ServerReceivedMessageCallback> serverReceivedMessageCallbacks;

        ISteamNetworkingSockets* m_sockets = nullptr;
        HSteamNetConnection m_connection = k_invalidConnection;

        inline static Client* s_callbackInstance = nullptr;

    public:
        Client() = default;
        Client(const Client&) = delete;
        Client& operator=(const Client&) = delete;
        Client(Client&&) = delete;
        Client& operator=(Client&&) = delete;

        ~Client();

        void addServerReceivedMessageCallback(const ServerReceivedMessageCallback& callback);

        zeus::expected<void, std::error_code> start();

        zeus::expected<void, std::error_code> connect(std::string_view ip, int port);

        zeus::expected<void, std::error_code> update(int maxMessagesToProcess);

        zeus::expected<void, std::error_code> disconnect(bool enableLinger);

        bool isConnected() const noexcept;

        HSteamNetConnection getServerHConn() const noexcept;

    private:
        void onConnectionStatusChanged(
            SteamNetConnectionStatusChangedCallback_t* info
        );

        static void connectionStatusChangedCallback(SteamNetConnectionStatusChangedCallback_t* info);
    };
}
#endif
