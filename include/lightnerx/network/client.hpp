#ifndef LIGHTNERX_NETWORK_CLIENT_HPP
#define LIGHTNERX_NETWORK_CLIENT_HPP

#include <deque>
#include <functional>
#include <string_view>

#include <lightnerx/Result.hpp>
#include <lightnerx/network/connection_handle.hpp>
#include <lightnerx/network/network_manager.hpp>

class ISteamNetworkingSockets;
struct SteamNetConnectionStatusChangedCallback_t;

namespace lightnerx::network {
    class Client {
    private:
        bool connected = false;
        bool updating = false;

        using ServerReceivedMessageCallback = std::function<void(std::string_view)>;
        std::deque<ServerReceivedMessageCallback> serverReceivedMessageCallbacks;

        ISteamNetworkingSockets* m_sockets = nullptr;
        ConnectionHandle m_connection = k_invalidConnection;

        inline static Client* s_callbackInstance = nullptr;

    public:
        Client() = default;
        Client(const Client&) = delete;
        Client& operator=(const Client&) = delete;
        Client(Client&&) = delete;
        Client& operator=(Client&&) = delete;

        ~Client();

        // The view borrows the receive buffer until the callback returns.
        void addServerReceivedMessageCallback(const ServerReceivedMessageCallback& callback);

        Result<void> start();

        Result<void> connect(std::string_view ip, int port);

        Result<void> update(int maxMessagesToProcess);

        Result<void> disconnect(bool enableLinger);

        bool isConnected() const noexcept;

        ConnectionHandle getServerHConn() const noexcept;

    private:
        void onConnectionStatusChanged(
            SteamNetConnectionStatusChangedCallback_t* info
        );

        static void connectionStatusChangedCallback(SteamNetConnectionStatusChangedCallback_t* info);
    };
}
#endif
