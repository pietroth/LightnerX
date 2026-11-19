#ifndef LIGHTNERX_NETWORK_NETWORK_MANAGER_HPP
#define LIGHTNERX_NETWORK_NETWORK_MANAGER_HPP

#include <steam/steamnetworkingsockets.h>
#include <zeus/expected.hpp>
#include <system_error>


namespace lightnerx::network {
    inline constexpr HSteamNetConnection k_invalidConnection = k_HSteamNetConnection_Invalid;
#if defined(_WIN32) && defined(LIGHTNERX_SHARED) && !defined(LightnerX_EXPORTS)
    __declspec(dllimport)
#endif
    extern bool g_bNetworkInitialized;
    bool networkInit();
    void networkKill();
    bool isNetworkInitialized();
    namespace detail {
        void setNetworkCleanup(void (*cleanup)() noexcept) noexcept;
        zeus::expected<void, std::error_code> acquireNetworkOwner(const void* owner);
        void releaseNetworkOwner(const void* owner, bool shutdown = true) noexcept;
    }
}

#endif
