#ifndef LIGHTNERX_NETWORK_NETWORK_MANAGER_HPP
#define LIGHTNERX_NETWORK_NETWORK_MANAGER_HPP

#include <steam/steamnetworkingsockets.h>
#include <expected>
#include <system_error>


namespace lightnerx::network {
    inline constexpr HSteamNetConnection k_invalidConnection = k_HSteamNetConnection_Invalid;
    extern bool g_bNetworkInitialized;
    bool networkInit();
    void networkKill();
    bool isNetworkInitialized();
    namespace detail {
        void setNetworkCleanup(void (*cleanup)() noexcept) noexcept;
        std::expected<void, std::error_code> acquireNetworkOwner(const void* owner);
        void releaseNetworkOwner(const void* owner) noexcept;
    }
}

#endif
