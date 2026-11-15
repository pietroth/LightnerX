#include <lightnerx/network/network_manager.hpp>

#include <lightnerx/compact/expected.hpp>
#include <system_error>

namespace lightnerx::network::detail {
    const void* networkOwner = nullptr;
    void (*networkCleanup)() noexcept = nullptr;
}

namespace lightnerx::network {
    bool g_bNetworkInitialized = false;

    bool networkInit() {
        if (g_bNetworkInitialized && SteamNetworkingSockets()) return true;
        g_bNetworkInitialized = false;

        SteamNetworkingErrMsg errMsg;
        if (GameNetworkingSockets_Init(nullptr, errMsg)) {
            g_bNetworkInitialized = true;
            return true;
        }
        return false;
    }

    void networkKill() {
        // A live wrapper owns the interface. Its release completes shutdown;
        // this also makes networkKill safe from an application callback.
        if (detail::networkOwner) return;
        if (detail::networkCleanup) detail::networkCleanup();
        if (!g_bNetworkInitialized) return;

        GameNetworkingSockets_Kill();
        g_bNetworkInitialized = false;
    }

    bool isNetworkInitialized() {
        return g_bNetworkInitialized && SteamNetworkingSockets() != nullptr;
    }

    namespace detail {
        void setNetworkCleanup(void (*cleanup)() noexcept) noexcept {
            networkCleanup = cleanup;
        }

        lightnerx::compact::expected<void, std::error_code> acquireNetworkOwner(const void* owner) {
            if (networkOwner) {
                return lightnerx::compact::unexpected(std::make_error_code(std::errc::device_or_resource_busy));
            }
            if (!networkInit()) {
                return lightnerx::compact::unexpected(std::make_error_code(std::errc::io_error));
            }
            networkOwner = owner;
            return {};
        }

        void releaseNetworkOwner(const void* owner) noexcept {
            if (networkOwner != owner) return;
            networkOwner = nullptr;
            networkKill();
        }
    }
}
