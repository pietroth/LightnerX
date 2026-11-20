#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

#include <lightnerx/network/client.hpp>

#include <steam/steamnetworkingsockets.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lightnerx::network {

Client::~Client() {
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

        m_sockets = nullptr;
        detail::releaseNetworkOwner(this);
    }

    connected = false;
}

void Client::addServerReceivedMessageCallback(const ServerReceivedMessageCallback& callback) {
    serverReceivedMessageCallbacks.push_back(callback);
}

Result<void> Client::start() {
    if (m_sockets) {
        return Result<void>::failure(ErrorCode::OperationInProgress);
    }

    const bool networkWasInitialized = isNetworkInitialized();
    if (auto acquired = detail::acquireNetworkOwner(this); !acquired) return acquired;

    m_sockets = SteamNetworkingSockets();

    if (!m_sockets) {
        detail::releaseNetworkOwner(this, !networkWasInitialized);

        return Result<void>::failure(ErrorCode::IoError);
    }

    s_callbackInstance = this;

    return {};
}

Result<void> Client::connect(std::string_view ip, int port) {
    if (!m_sockets) {
        return Result<void>::failure(ErrorCode::NotConnected);
    }

    if (m_connection != k_invalidConnection) {
        return Result<void>::failure(ErrorCode::AlreadyConnected);
    }

    if (ip.empty() || ip.find('\0') != std::string_view::npos || port < 1 || port > 65535) {
        return Result<void>::failure(ErrorCode::InvalidArgument);
    }

    SteamNetworkingIPAddr address{};

    const bool bracketed = ip.size() >= 2 && ip.front() == '[' && ip.back() == ']';
    const std::string literal{bracketed ? ip.substr(1, ip.size() - 2) : ip};
    in_addr ipv4{};
    in6_addr ipv6{};
    const auto destinationPort = static_cast<uint16>(port);
    if (!bracketed && inet_pton(AF_INET, literal.c_str(), &ipv4) == 1) {
        address.SetIPv4(ntohl(ipv4.s_addr), destinationPort);
    } else if (inet_pton(AF_INET6, literal.c_str(), &ipv6) == 1) {
        address.SetIPv6(ipv6.s6_addr, destinationPort);
    } else {
        return Result<void>::failure(ErrorCode::InvalidArgument);
    }

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

    if (m_connection == k_invalidConnection) {
        return Result<void>::failure(ErrorCode::IoError);
    }

    return {};
}

Result<void> Client::update(int maxMessagesToProcess) {
    if (!m_sockets) {
        return Result<void>::failure(ErrorCode::NotConnected);
    }

    if (updating) {
        return Result<void>::failure(ErrorCode::OperationInProgress);
    }
    updating = true;
    struct UpdateGuard {
        bool& flag;
        ~UpdateGuard() { flag = false; }
    } updateGuard{updating};

    m_sockets->RunCallbacks();

    if (maxMessagesToProcess <= 0) {
        return {};
    }

    if (m_connection == k_invalidConnection) {
        connected = false;
        return {};
    }

    std::vector<SteamNetworkingMessage_t*> messages(
        static_cast<std::size_t>(maxMessagesToProcess)
    );
    struct ReleaseMessages {
        std::vector<SteamNetworkingMessage_t*>& messages;
        ~ReleaseMessages() {
            for (auto* message : messages) if (message) message->Release();
        }
    } releaseMessages{messages};

    const int count = m_sockets->ReceiveMessagesOnConnection(
        m_connection,
        messages.data(),
        maxMessagesToProcess
    );

    if (count < 0) {
        const auto connection = std::exchange(m_connection, k_invalidConnection);
        connected = false;
        m_sockets->CloseConnection(connection, 0, nullptr, false);
        return Result<void>::failure(ErrorCode::IoError);
    }
    if (count == 0) {
        return {};
    }

    const auto receivingConnection = m_connection;
    for (int i = 0; i < count && m_connection == receivingConnection; ++i) {
        SteamNetworkingMessage_t* message = messages[i];

        std::string_view convertedMessage{
            static_cast<const char*>(message->m_pData),
            static_cast<std::size_t>(message->m_cbSize)
        };

        const auto callbackCount = serverReceivedMessageCallbacks.size();
        for (std::size_t j = 0; j < callbackCount && m_connection == receivingConnection; ++j) {
            serverReceivedMessageCallbacks[j](convertedMessage);
        }
        message->Release();
        messages[i] = nullptr;
    }

    return {};
}

Result<void> Client::disconnect(bool enableLinger) {
    if (!m_sockets || m_connection == k_invalidConnection) {
        return Result<void>::failure(ErrorCode::NotConnected);
    }

    if
    (m_sockets->CloseConnection(
        m_connection,
        k_ESteamNetConnectionEnd_App_Generic,
        nullptr,
        enableLinger
    )) {
        m_connection = k_invalidConnection;
        connected = false;
        return {};
    }

    return Result<void>::failure(ErrorCode::IoError);
}

bool Client::isConnected() const noexcept {
    return connected;
}

ConnectionHandle Client::getServerHConn() const noexcept {
    return m_connection;
}

void Client::onConnectionStatusChanged(
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

void Client::connectionStatusChangedCallback(SteamNetConnectionStatusChangedCallback_t* info) {
    if (s_callbackInstance) {
        s_callbackInstance->onConnectionStatusChanged(info);
    }
}

}

