module;

#include "steam/isteamnetworkingsockets.h"
#include "steam/steamnetworkingtypes.h"
#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>

export module lightnerx.network.client;

import std;

import lightnerx.network.network_manager;

export namespace lightnerx::network {
    constexpr HSteamNetConnection k_invalidConnection = 0;

    class Client {
    private:
        bool connected = false;

        using ServerReceivedMessageCallback = std::function<void(std::span<const std::byte>)>;
        std::vector<ServerReceivedMessageCallback> serverReceivedMessageCallbacks;

        ISteamNetworkingSockets* m_sockets = nullptr;
        HSteamNetConnection m_connection = k_invalidConnection;

        inline static Client* s_callbackInstance = nullptr;

    public:
        ~Client() {
            if (s_callbackInstance == this) {
                s_callbackInstance = nullptr;
            }

            if (m_sockets) {
                if (m_connection != k_invalidConnection) {
                    m_sockets->CloseConnection(
                        m_connection,
                        0,
                        nullptr,
                        false
                    );

                    m_connection = k_invalidConnection;
                }

                GameNetworkingSockets_Kill();
                m_sockets = nullptr;
            }

            connected = false;
        }

        void addServerReceivedMessageCallback(const ServerReceivedMessageCallback& callback) {
            serverReceivedMessageCallbacks.push_back(callback);
        }

        std::expected<void, std::error_code> start() {
            if (m_sockets) {
                return std::unexpected(
                    std::make_error_code(std::errc::operation_in_progress)
                );
            }

            if (s_callbackInstance && s_callbackInstance != this) {
                return std::unexpected(
                    std::make_error_code(std::errc::device_or_resource_busy)
                );
            }

            if (!lightnerx::network::networkInit()) {
                return std::unexpected(
                    std::make_error_code(std::errc::io_error)
                );
            }

            m_sockets = SteamNetworkingSockets();

            if (!m_sockets) {
                GameNetworkingSockets_Kill();

                return std::unexpected(
                    std::make_error_code(std::errc::io_error)
                );
            }

            s_callbackInstance = this;

            return {};
        }

        std::expected<void, std::error_code> connect(std::string_view ip, int port) {
            if (!m_sockets) {
                return std::unexpected(std::make_error_code(std::errc::not_connected));
            }

            if (m_connection != k_invalidConnection) {
                return std::unexpected(std::make_error_code(std::errc::already_connected));
            }

            if (ip.empty() || port < 1 || port > 65535) {
                return std::unexpected(std::make_error_code(std::errc::invalid_argument));
            }

            SteamNetworkingIPAddr address{};

            const std::string ip_string{ip};

            if (!address.ParseString(ip_string.c_str())) {
                return std::unexpected(
                    std::make_error_code(std::errc::invalid_argument)
                );
            }

            address.m_port = static_cast<uint16>(port);

            SteamNetworkingConfigValue_t option{};

            option.SetPtr(
                k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
                reinterpret_cast<void*>(connectionStatusChangedCallback)
            );

            m_connection = m_sockets->ConnectByIPAddress(
                address,
                1,
                &option
            );

            if (m_connection == k_HSteamNetConnection_Invalid) {
                return std::unexpected(
                    std::make_error_code(std::errc::io_error)
                );
            }

            return {};
        }

        std::expected<void, std::error_code> update(int maxMessagesToProcess) {
            if (!m_sockets) {
                return std::unexpected(std::make_error_code(std::errc::not_connected));
            }

            m_sockets->RunCallbacks();

            if (maxMessagesToProcess <= 0) {
                return {};
            }

            if (m_connection == k_HSteamNetConnection_Invalid) {
                connected = false;
                return {};
            }

            std::vector<SteamNetworkingMessage_t*> messages(
                static_cast<std::size_t>(maxMessagesToProcess)
            );


            const int count = m_sockets->ReceiveMessagesOnConnection(
                m_connection,
                messages.data(),
                maxMessagesToProcess
            );

            if (count == 0) {
                return {};
            }

            for (int i = 0; i < count; ++i) {
                SteamNetworkingMessage_t* message = messages[i];

                std::span<const std::byte> convertedMessage{
                    static_cast<const std::byte*>(message->m_pData),
                    static_cast<std::size_t>(message->m_cbSize)
                };

                for (const auto& callback : serverReceivedMessageCallbacks) {
                    callback(convertedMessage);
                }

                message->Release();
            }

            return {};
        }

        std::expected<void, std::error_code> disconnect(bool enableLinger) {
            if (!lightnerx::network::isNetworkInitialized()) {
                return std::unexpected(
                    std::make_error_code(std::errc::not_connected)
                );
            }

            if
            (m_sockets->CloseConnection(
                m_connection,
                k_ESteamNetConnectionEnd_App_Generic,
                nullptr,
                enableLinger
            )) {
                return {};
            }

            return std::unexpected(std::make_error_code(std::errc::io_error));
        }

        bool isConnected() const noexcept {
            return connected;
        }

        HSteamNetConnection getServerHConn() const noexcept {
            return m_connection;
        }

    private:
        void onConnectionStatusChanged(
            SteamNetConnectionStatusChangedCallback_t* info
        ) {
            if (!info || info->m_hConn != m_connection) {
                return;
            }

            switch (info->m_info.m_eState) {
                case k_ESteamNetworkingConnectionState_Connecting:
                case k_ESteamNetworkingConnectionState_FindingRoute:
                    connected = false;
                    break;

                case k_ESteamNetworkingConnectionState_Connected:
                    connected = true;
                    break;

                case k_ESteamNetworkingConnectionState_ClosedByPeer:
                case k_ESteamNetworkingConnectionState_ProblemDetectedLocally: {
                    connected = false;

                    const auto connection = m_connection;
                    m_connection = k_invalidConnection;

                    m_sockets->CloseConnection(
                        connection,
                        0,
                        nullptr,
                        false
                    );

                    break;
                }

                default:
                    break;
            }
        }

        static void connectionStatusChangedCallback(SteamNetConnectionStatusChangedCallback_t* info) {
            if (s_callbackInstance) {
                s_callbackInstance->onConnectionStatusChanged(info);
            }
        }
    };
}
