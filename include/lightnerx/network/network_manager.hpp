#ifndef LIGHTNERX_NETWORK_NETWORK_MANAGER_HPP
#define LIGHTNERX_NETWORK_NETWORK_MANAGER_HPP

#include <lightnerx/Result.hpp>
#include <lightnerx/network/connection_handle.hpp>

namespace lightnerx::network {
#if defined(_WIN32) && defined(LIGHTNERX_SHARED) && !defined(LightnerX_EXPORTS)
    __declspec(dllimport)
#endif
    extern bool g_bNetworkInitialized;
    bool networkInit();
    void networkKill();
    bool isNetworkInitialized();
    namespace detail {
        void setNetworkCleanup(void (*cleanup)() noexcept) noexcept;
        Result<void> acquireNetworkOwner(const void* owner);
        void releaseNetworkOwner(const void* owner, bool shutdown = true) noexcept;
    }
}

#endif
