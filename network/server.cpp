#include <lightnerx/network/server.hpp>

#include <steam/steamnetworkingsockets.h>

#include <cstddef>
#include <cstdint>
#include <memory>

namespace lightnerx::network {

Server::~Server() { close(); }

void Server::addClientConnectedCallback(ClientConnectedCallback callback) {
    clientConnectedCallbacks.push_back(callback);
}

void Server::addClientDisconnectedCallback(ClientDisconnectedCallback callback) {
    clientDisconnectedCallbacks.push_back(callback);
}

void Server::addClientReceivedMessageCallback(ClientReceivedMessageCallback callback) {
    clientReceivedMessageCallbacks.push_back(callback);
}

Result<void> Server::start(int port, std::uint32_t timeoutConnected) {
    if (m_pInterface) {
        return Result<void>::failure(ErrorCode::OperationInProgress);
    }
    if (port < 1 || port > 65535 || timeoutConnected > INT32_MAX) {
        return Result<void>::failure(ErrorCode::InvalidArgument);
    }
    const bool networkWasInitialized = isNetworkInitialized();
    if (auto acquired = detail::acquireNetworkOwner(this); !acquired) return acquired;

    m_pInterface = SteamNetworkingSockets();
    if (!m_pInterface) {
        detail::releaseNetworkOwner(this, !networkWasInitialized);
        return Result<void>::failure(ErrorCode::IoError);
    }
    closeRequested = false;

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
        closeResources(!networkWasInitialized);
        return Result<void>::failure(ErrorCode::ResourceUnavailableTryAgain);
    }

    m_hPollGroup = m_pInterface->CreatePollGroup();

    if (m_hPollGroup == k_invalidPollGroup) {
        closeResources(!networkWasInitialized);
        return Result<void>::failure(ErrorCode::ResourceUnavailableTryAgain);
    }

    return {};
}

Result<void> Server::update() {
    if (!m_pInterface) {
        return Result<void>::failure(ErrorCode::NotConnected);
    }
    if (updating || activeCalls != 0) {
        return Result<void>::failure(ErrorCode::OperationInProgress);
    }
    ActiveCall active{*this};
    updating = true;
    struct UpdateGuard {
        bool& flag;
        ~UpdateGuard() { flag = false; }
    } updateGuard{updating};
    m_pInterface->RunCallbacks();
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
        return Result<void>::failure(ErrorCode::IoError);
    }

    if (received > 0) {
        const auto count = clientReceivedMessageCallbacks.size();
        for (std::size_t i = 0; i < count && !closeRequested; ++i) {
            clientReceivedMessageCallbacks[i]({
                static_cast<const char*>(message->m_pData),
                static_cast<std::size_t>(message->m_cbSize)
                },
                message->m_conn
            );
        }
    }

    return {};
}

Result<void> Server::disconnectConnection(
            ConnectionHandle hConn, bool enableLinger
        ) {
    if (!m_pInterface || closeRequested) {
        return Result<void>::failure(ErrorCode::NotConnected);
    }

    ActiveCall active{*this};
    ISteamNetworkingSockets* sockets = m_pInterface;

    if (!sockets->CloseConnection(
        hConn,
        k_ESteamNetConnectionEnd_App_Generic,
        nullptr,
        enableLinger
    )) {
        return Result<void>::failure(ErrorCode::IoError);
    }

    const auto count = clientDisconnectedCallbacks.size();
    for (std::size_t i = 0; i < count && !closeRequested; ++i) {
        clientDisconnectedCallbacks[i](hConn);
    }

    return {};
}

void Server::close() noexcept {
    if (activeCalls != 0) {
        closeRequested = true;
        return;
    }
    closeResources(true);
}

void Server::closeResources(bool shutdown) noexcept {
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

    detail::releaseNetworkOwner(this, shutdown);
}

void Server::SteamNetConnectionStatusChangedCallback(
                SteamNetConnectionStatusChangedCallback_t* pInfo
            ) noexcept {
    auto* instance = s_pCallbackInstance;
    if (instance != nullptr && !instance->closeRequested) {
        instance->OnConnectionStatusChanged(pInfo);
    }
}

void Server::OnConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* pInfo) {
    if (!pInfo || pInfo->m_info.m_hListenSocket != m_hListenSocket) return;
    ConnectionHandle hConn = pInfo->m_hConn;
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
                if (i != 0 && !m_pInterface->GetConnectionInfo(hConn, &info)) {
                    break;
                }
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
            const ConnectionHandle hConn = pInfo->m_hConn;
            if (!m_pInterface->CloseConnection(hConn, 0, nullptr, false)) break;

            const auto count = clientDisconnectedCallbacks.size();
            for (std::size_t i = 0; i < count && !closeRequested; ++i) {
                clientDisconnectedCallbacks[i](hConn);
            }

            break;
        }
    }
}

}

