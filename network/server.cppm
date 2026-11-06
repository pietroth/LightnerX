module;

#include "steam/isteamnetworkingsockets.h"
#include "steam/steamnetworkingtypes.h"
#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>

export module network.server;

import std;

import network.network_manager;

export namespace network {
    constexpr HSteamListenSocket k_invalidSocket = 0;
    constexpr HSteamNetPollGroup k_invalidPollGroup = 0;

    class Server {
    private:
        ISteamNetworkingSockets* m_pInterface = nullptr;
        HSteamListenSocket m_hListenSocket = k_invalidSocket;
        HSteamNetPollGroup m_hPollGroup = k_invalidPollGroup;

        using ClientConnectedCallback = std::function<void(HSteamNetConnection)>;
        std::vector<ClientConnectedCallback> clientConnectedCallbacks;

        using ClientDisconnectedCallback = std::function<void(HSteamNetConnection)>;
        std::vector<ClientDisconnectedCallback> clientDisconnectedCallbacks;

        using ClientReceivedMessageCallback = std::function<void(std::span<const std::byte>, HSteamNetConnection)>;
        std::vector<ClientReceivedMessageCallback> clientReceivedMessageCallbacks;

    public:
        void addClientConnectedCallback(ClientConnectedCallback callback) {
            clientConnectedCallbacks.push_back(callback);
        }

        void addClientDisconnectedCallback(ClientDisconnectedCallback callback) {
            clientDisconnectedCallbacks.push_back(callback);
        }

        void addClientReceivedMessageCallback(ClientReceivedMessageCallback callback) {
            clientReceivedMessageCallbacks.push_back(callback);
        }

        std::expected<void, std::error_code> start(int port, uint32_t timeoutConnected) {
            if (!network::networkInit()) {
                return std::unexpected(
                    std::make_error_code(std::errc::io_error)
                );
            }

            m_pInterface = SteamNetworkingSockets();

            SteamNetworkingIPAddr address;
            address.Clear();
            address.m_port = port;

            s_pCallbackInstance = this;

            SteamNetworkingConfigValue_t options[2];

            options[0].SetPtr(
                k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
                reinterpret_cast<void*>(SteamNetConnectionStatusChangedCallback)
            );

            options[1].SetInt32(
                k_ESteamNetworkingConfig_TimeoutConnected,
                timeoutConnected
            );

            m_hListenSocket = m_pInterface->CreateListenSocketIP(
                address,
                2,
                options
            );

            if (m_hListenSocket == k_invalidSocket) {
                s_pCallbackInstance = nullptr;
                GameNetworkingSockets_Kill();

                std::println(std::cerr, "Failed to create listen socket.");
                return std::unexpected(std::make_error_code(std::errc::resource_unavailable_try_again));
            }

            m_hPollGroup = m_pInterface->CreatePollGroup();

            if (m_hPollGroup == k_invalidPollGroup) {
                s_pCallbackInstance = nullptr;
                m_pInterface->CloseListenSocket(m_hListenSocket);
                GameNetworkingSockets_Kill();

                std::println(std::cerr, "Failed to create poll group.");
                return std::unexpected(std::make_error_code(std::errc::resource_unavailable_try_again));
            }

            return {};
        }

        std::expected<void, std::error_code> update() {
            m_pInterface->RunCallbacks();

            ISteamNetworkingMessage* message = nullptr;

            int received = m_pInterface->ReceiveMessagesOnPollGroup(
                m_hPollGroup,
                &message,
                1
            );

            if (received > 0) {
                for (const auto& callback : clientReceivedMessageCallbacks) {
                    callback({
                        static_cast<const std::byte*>(message->m_pData),
                        static_cast<std::size_t>(message->m_cbSize)
                        },
                        message->m_conn
                    );
                }
                message->Release();
            }

            return {};
        }

        std::expected<void, std::error_code> disconnectConnection(
            HSteamNetConnection hConn, bool enableLinger
        ) {
            if (!network::isNetworkInitialized()) {
                return std::unexpected(
                    std::make_error_code(std::errc::not_connected)
                );
            }

            ISteamNetworkingSockets* sockets = SteamNetworkingSockets();

            if (!sockets->CloseConnection(
                hConn,
                k_ESteamNetConnectionEnd_App_Generic,
                nullptr,
                enableLinger
            )) {
                return std::unexpected(
                    std::make_error_code(std::errc::io_error)
                );;
            }

            for (const auto& callback : clientDisconnectedCallbacks) {
                callback(hConn);
            }

            return {};
        }

        void close() {
            if (m_pInterface == nullptr) {
                return;
            }

            if (m_hListenSocket != k_invalidSocket) {
                m_pInterface->CloseListenSocket(m_hListenSocket);
                m_hListenSocket = k_invalidSocket;
            }

            if (m_hPollGroup != k_invalidPollGroup) {
                m_pInterface->DestroyPollGroup(m_hPollGroup);
                m_hPollGroup = k_invalidPollGroup;
            }

            m_pInterface = nullptr;
            s_pCallbackInstance = nullptr;

            GameNetworkingSockets_Kill();
        }

        private:
            inline static Server* s_pCallbackInstance = nullptr;

            static void SteamNetConnectionStatusChangedCallback(
                SteamNetConnectionStatusChangedCallback_t* pInfo
            ) {
                if (s_pCallbackInstance != nullptr) {
                    s_pCallbackInstance->OnConnectionStatusChanged(pInfo);
                }
            }

            void OnConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* pInfo) {
                HSteamNetConnection hConn = pInfo->m_hConn;
                SteamNetConnectionInfo_t info = pInfo->m_info;

                switch (info.m_eState) {
                    case k_ESteamNetworkingConnectionState_None:
                            break;

                    case k_ESteamNetworkingConnectionState_Connecting:
                        if (m_pInterface->AcceptConnection(hConn) != k_EResultOK) {
                            m_pInterface->CloseConnection(hConn, 0, nullptr, false);
                            break;
                        }

                        if (!m_pInterface->SetConnectionPollGroup(hConn, m_hPollGroup)) {
                            m_pInterface->CloseConnection(hConn, 0, nullptr, false);
                            break;
                        }

                        for (const auto& clientConnectedCallback : clientConnectedCallbacks) {
                            clientConnectedCallback(hConn);
                        }

                        break;

                    case k_ESteamNetworkingConnectionState_FindingRoute:
                    case k_ESteamNetworkingConnectionState_FinWait:
                    case k_ESteamNetworkingConnectionState_Linger:
                    case k_ESteamNetworkingConnectionState_Dead:
                    case k_ESteamNetworkingConnectionState__Force32Bit:
                    case k_ESteamNetworkingConnectionState_Connected:
                        break;

                    case k_ESteamNetworkingConnectionState_ClosedByPeer:
                    case k_ESteamNetworkingConnectionState_ProblemDetectedLocally : {
                        const HSteamNetConnection hConn = pInfo->m_hConn;
                        m_pInterface->CloseConnection(hConn, 0, nullptr, false);

                        for (const auto& callback : clientDisconnectedCallbacks) {
                            callback(hConn);
                        }

                        break;
                    }
                }
            }
    };

}
