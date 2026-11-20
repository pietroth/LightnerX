#ifndef LIGHTNERX_NETWORK_SERVER_HPP
#define LIGHTNERX_NETWORK_SERVER_HPP

#include <cstdint>
#include <deque>
#include <functional>
#include <string_view>

#include <lightnerx/Result.hpp>
#include <lightnerx/network/connection_handle.hpp>
#include <lightnerx/network/network_manager.hpp>

class ISteamNetworkingSockets;
struct SteamNetConnectionStatusChangedCallback_t;

namespace lightnerx::network {
    inline constexpr std::uint32_t k_invalidSocket = 0;
    inline constexpr std::uint32_t k_invalidPollGroup = 0;

    class Server {
    private:
        ISteamNetworkingSockets* m_pInterface = nullptr;
        std::uint32_t m_hListenSocket = k_invalidSocket;
        std::uint32_t m_hPollGroup = k_invalidPollGroup;
        bool updating = false;
        bool closeRequested = false;
        unsigned activeCalls = 0;

        struct ActiveCall {
            Server& server;
            explicit ActiveCall(Server& value) : server(value) { ++server.activeCalls; }
            ~ActiveCall() {
                if (--server.activeCalls == 0 && server.closeRequested) server.close();
            }
        };

        using ClientConnectedCallback = std::function<void(ConnectionHandle)>;
        std::deque<ClientConnectedCallback> clientConnectedCallbacks;

        using ClientDisconnectedCallback = std::function<void(ConnectionHandle)>;
        std::deque<ClientDisconnectedCallback> clientDisconnectedCallbacks;

        using ClientReceivedMessageCallback = std::function<void(std::string_view, ConnectionHandle)>;
        std::deque<ClientReceivedMessageCallback> clientReceivedMessageCallbacks;

    public:
        Server() = default;
        Server(const Server&) = delete;
        Server& operator=(const Server&) = delete;
        Server(Server&&) = delete;
        Server& operator=(Server&&) = delete;
        ~Server();

        // Connection status callbacks run inside the transport and must not throw.
        void addClientConnectedCallback(ClientConnectedCallback callback);

        void addClientDisconnectedCallback(ClientDisconnectedCallback callback);

        // The view borrows the receive buffer until the callback returns.
        void addClientReceivedMessageCallback(ClientReceivedMessageCallback callback);

        Result<void> start(int port, std::uint32_t timeoutConnected);

        Result<void> update();

        Result<void> disconnectConnection(
            ConnectionHandle hConn, bool enableLinger
        );

        void close() noexcept;

        private:
            void closeResources(bool shutdown) noexcept;

            inline static Server* s_pCallbackInstance = nullptr;

            static void SteamNetConnectionStatusChangedCallback(
                SteamNetConnectionStatusChangedCallback_t* pInfo
            ) noexcept;

            void OnConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* pInfo);
    };

}
#endif
