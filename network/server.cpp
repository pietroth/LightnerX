#include <lightnerx/network/server.hpp>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <zeus/expected.hpp>
#include <memory>
#include <system_error>
#include <utility>

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

zeus::expected<void, std::error_code> Server::start(int port, uint32_t timeoutConnected) {
    if (m_pInterface) {
        return zeus::unexpected(std::make_error_code(std::errc::operation_in_progress));
    }
    if (port < 1 || port > 65535 || timeoutConnected > INT32_MAX) {
        return zeus::unexpected(std::make_error_code(std::errc::invalid_argument));
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
        return zeus::unexpected(std::make_error_code(std::errc::resource_unavailable_try_again));
    }

    m_hPollGroup = m_pInterface->CreatePollGroup();

    if (m_hPollGroup == k_invalidPollGroup) {
        close();
        return zeus::unexpected(std::make_error_code(std::errc::resource_unavailable_try_again));
    }

    return {};
}

zeus::expected<void, std::error_code> Server::update() {
    if (!m_pInterface) {
        return zeus::unexpected(std::make_error_code(std::errc::not_connected));
    }
    if (updating || activeCalls != 0) {
        return zeus::unexpected(std::make_error_code(std::errc::operation_in_progress));
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
        return zeus::unexpected(std::make_error_code(std::errc::io_error));
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

zeus::expected<void, std::error_code> Server::disconnectConnection(
            HSteamNetConnection hConn, bool enableLinger
        ) {
    if (!m_pInterface || closeRequested) {
        return zeus::unexpected(
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
        return zeus::unexpected(
            std::make_error_code(std::errc::io_error)
        );
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

void Server::SteamNetConnectionStatusChangedCallback(
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

void Server::OnConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* pInfo) {
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

}

