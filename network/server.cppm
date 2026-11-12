module;

#include "steam/isteamnetworkingsockets.h"
#include "steam/steamnetworkingtypes.h"
#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>

export module lightnerx.network.server;

import std;

import lightnerx.network.network_manager;

export namespace lightnerx::network {
    constexpr HSteamListenSocket k_invalidSocket = 0;
    constexpr HSteamNetPollGroup k_invalidPollGroup = 0;

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

        using ClientReceivedMessageCallback = std::function<void(std::span<const std::byte>, HSteamNetConnection)>;
        std::deque<ClientReceivedMessageCallback> clientReceivedMessageCallbacks;

    public:
        Server() = default;
        Server(const Server&) = delete;
        Server& operator=(const Server&) = delete;
        Server(Server&&) = delete;
        Server& operator=(Server&&) = delete;
        ~Server() { close(); }

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
            if (m_pInterface) {
                return std::unexpected(std::make_error_code(std::errc::operation_in_progress));
            }
            if (port < 1 || port > 65535 || timeoutConnected > INT32_MAX) {
                return std::unexpected(std::make_error_code(std::errc::invalid_argument));
            }
            if (auto acquired = detail::acquireNetworkOwner(this); !acquired) return acquired;

            m_pInterface = SteamNetworkingSockets();
            closeRequested = false;
            callbackException = nullptr;

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
                close();
                return std::unexpected(std::make_error_code(std::errc::resource_unavailable_try_again));
            }

            m_hPollGroup = m_pInterface->CreatePollGroup();

            if (m_hPollGroup == k_invalidPollGroup) {
                close();
                return std::unexpected(std::make_error_code(std::errc::resource_unavailable_try_again));
            }

            return {};
        }

        std::expected<void, std::error_code> update() {
            if (!m_pInterface) {
                return std::unexpected(std::make_error_code(std::errc::not_connected));
            }
            if (updating || activeCalls != 0) {
                return std::unexpected(std::make_error_code(std::errc::operation_in_progress));
            }
            ActiveCall active{*this};
            updating = true;
            struct UpdateGuard {
                bool& flag;
                ~UpdateGuard() { flag = false; }
            } updateGuard{updating};
            m_pInterface->RunCallbacks();
            if (callbackException) {
                std::rethrow_exception(std::exchange(callbackException, nullptr));
            }
            if (closeRequested) return {};

            ISteamNetworkingMessage* message = nullptr;

            int received = m_pInterface->ReceiveMessagesOnPollGroup(
                m_hPollGroup,
                &message,
                1
            );

            const auto release = [](ISteamNetworkingMessage* value) { if (value) value->Release(); };
            std::unique_ptr<ISteamNetworkingMessage, decltype(release)> ownedMessage(message, release);
            if (received < 0) {
                return std::unexpected(std::make_error_code(std::errc::io_error));
            }

            if (received > 0) {
                const auto count = clientReceivedMessageCallbacks.size();
                for (std::size_t i = 0; i < count && !closeRequested; ++i) {
                    clientReceivedMessageCallbacks[i]({
                        static_cast<const std::byte*>(message->m_pData),
                        static_cast<std::size_t>(message->m_cbSize)
                        },
                        message->m_conn
                    );
                }
            }

            return {};
        }

        std::expected<void, std::error_code> disconnectConnection(
            HSteamNetConnection hConn, bool enableLinger
        ) {
            if (!m_pInterface || closeRequested) {
                return std::unexpected(
                    std::make_error_code(std::errc::not_connected)
                );
            }

            ActiveCall active{*this};
            ISteamNetworkingSockets* sockets = m_pInterface;

            if (!sockets->CloseConnection(
                hConn,
                k_ESteamNetConnectionEnd_App_Generic,
                nullptr,
                enableLinger
            )) {
                return std::unexpected(
                    std::make_error_code(std::errc::io_error)
                );
            }

            const auto count = clientDisconnectedCallbacks.size();
            for (std::size_t i = 0; i < count && !closeRequested; ++i) {
                clientDisconnectedCallbacks[i](hConn);
            }

            return {};
        }

        void close() noexcept {
            if (activeCalls != 0) {
                closeRequested = true;
                return;
            }
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
            if (s_pCallbackInstance == this) s_pCallbackInstance = nullptr;
            closeRequested = false;
            callbackException = nullptr;

            detail::releaseNetworkOwner(this);
        }

        private:
            inline static Server* s_pCallbackInstance = nullptr;

            static void SteamNetConnectionStatusChangedCallback(
                SteamNetConnectionStatusChangedCallback_t* pInfo
            ) noexcept {
                auto* instance = s_pCallbackInstance;
                if (instance != nullptr && !instance->closeRequested) {
                    try {
                        instance->OnConnectionStatusChanged(pInfo);
                    } catch (...) {
                        // Never unwind through GNS. update() rethrows after RunCallbacks.
                        if (!instance->callbackException) instance->callbackException = std::current_exception();
                    }
                }
            }

            void OnConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* pInfo) {
                if (!pInfo || pInfo->m_info.m_hListenSocket != m_hListenSocket) return;
                HSteamNetConnection hConn = pInfo->m_hConn;
                SteamNetConnectionInfo_t info = pInfo->m_info;

                switch (info.m_eState) {
                    case k_ESteamNetworkingConnectionState_None:
                            break;

                    case k_ESteamNetworkingConnectionState_Connecting: {
                        if (m_pInterface->AcceptConnection(hConn) != k_EResultOK) {
                            m_pInterface->CloseConnection(hConn, 0, nullptr, false);
                            break;
                        }

                        if (!m_pInterface->SetConnectionPollGroup(hConn, m_hPollGroup)) {
                            m_pInterface->CloseConnection(hConn, 0, nullptr, false);
                            break;
                        }

                        const auto count = clientConnectedCallbacks.size();
                        for (std::size_t i = 0; i < count && !closeRequested; ++i) {
                            clientConnectedCallbacks[i](hConn);
                        }

                        break;
                    }

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
                        if (!m_pInterface->CloseConnection(hConn, 0, nullptr, false)) break;

                        const auto count = clientDisconnectedCallbacks.size();
                        for (std::size_t i = 0; i < count && !closeRequested; ++i) {
                            clientDisconnectedCallbacks[i](hConn);
                        }

                        break;
                    }
                }
            }
    };

}
