#ifndef LIGHTNERX_NETWORK_SERVER_HPP
#define LIGHTNERX_NETWORK_SERVER_HPP

#include "steam/isteamnetworkingsockets.h"
#include "steam/steamnetworkingtypes.h"
#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <lightnerx/compact/expected.hpp>
#include <functional>
#include <tcb/span.hpp>
#include <system_error>

#include <lightnerx/network/network_manager.hpp>

namespace lightnerx::network {
    inline constexpr HSteamListenSocket k_invalidSocket = 0;
    inline constexpr HSteamNetPollGroup k_invalidPollGroup = 0;

    class Server {
    private:
        ISteamNetworkingSockets* m_pInterface = nullptr;
        HSteamListenSocket m_hListenSocket = k_invalidSocket;
        HSteamNetPollGroup m_hPollGroup = k_invalidPollGroup;
        bool updating = false;
        bool closeRequested = false;
        unsigned activeCalls = 0;
        std::exception_ptr callbackException;

        struct ActiveCall {
            Server& server;
            explicit ActiveCall(Server& value) : server(value) { ++server.activeCalls; }
            ~ActiveCall() {
                if (--server.activeCalls == 0 && server.closeRequested) server.close();
            }
        };

        using ClientConnectedCallback = std::function<void(HSteamNetConnection)>;
        std::deque<ClientConnectedCallback> clientConnectedCallbacks;

        using ClientDisconnectedCallback = std::function<void(HSteamNetConnection)>;
        std::deque<ClientDisconnectedCallback> clientDisconnectedCallbacks;

        using ClientReceivedMessageCallback = std::function<void(tcb::span<const std::byte>, HSteamNetConnection)>;
        std::deque<ClientReceivedMessageCallback> clientReceivedMessageCallbacks;

    public:
        Server() = default;
        Server(const Server&) = delete;
        Server& operator=(const Server&) = delete;
        Server(Server&&) = delete;
        Server& operator=(Server&&) = delete;
        ~Server();

        void addClientConnectedCallback(ClientConnectedCallback callback);

        void addClientDisconnectedCallback(ClientDisconnectedCallback callback);

        void addClientReceivedMessageCallback(ClientReceivedMessageCallback callback);

        lightnerx::compact::expected<void, std::error_code> start(int port, uint32_t timeoutConnected);

        lightnerx::compact::expected<void, std::error_code> update();

        lightnerx::compact::expected<void, std::error_code> disconnectConnection(
            HSteamNetConnection hConn, bool enableLinger
        );

        void close() noexcept;

        private:
            inline static Server* s_pCallbackInstance = nullptr;

            static void SteamNetConnectionStatusChangedCallback(
                SteamNetConnectionStatusChangedCallback_t* pInfo
            ) noexcept;

            void OnConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* pInfo);
    };

}
#endif
