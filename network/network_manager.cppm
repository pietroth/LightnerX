module;

#include <steam/steamnetworkingsockets.h>

export module lightnerx.network.network_manager;

export namespace lightnerx::network {
    bool g_bNetworkInitialized = false;

    bool networkInit() {
        if (g_bNetworkInitialized) return true;

        SteamNetworkingErrMsg errMsg;
        if (GameNetworkingSockets_Init(nullptr, errMsg)) {
            g_bNetworkInitialized = true;
            return true;
        }
        return false;
    }

    void networkKill() {
        if (!g_bNetworkInitialized) return;

        GameNetworkingSockets_Kill();
        g_bNetworkInitialized = false;
    }

    bool isNetworkInitialized() {
        return g_bNetworkInitialized;
    }
}
